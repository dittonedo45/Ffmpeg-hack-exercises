#include <thread>
#include <iostream>
#include <exception>
#include <vector>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <pthread.h>
#include <python3.8/Python.h>
  static int Random (int a, int b)
  {
    if (a == b)
      return a;
    else
      {
	return (rand () % abs (b - a)) + (a < b ? a : b);
      }
  }
  pthread_mutex_t pyU = PTHREAD_MUTEX_INITIALIZER;
};

using namespace std;
struct FrissonInputError:public exception
{
};
struct EncoderError:public FrissonInputError
{
};
struct FrissonDeckError:public exception
{
};
struct FilterError:exception
{
};

struct FrissonInput
{
  AVFormatContext *fctx;
  AVCodecContext *dec;
  int stream;
  uint64_t duration;
  string filename;

    FrissonInput (string file):
	    fctx (0),
	    dec (0),
	    stream (-1),
	    duration (0),
	    filename (file)
  {
    int r (0);
    AVCodec *d = NULL;

      r = avformat_open_input (&fctx, file.c_str (), NULL, NULL);

    if (r < 0)
      {
	throw FrissonInputError ();
      }

    do
      {
	r = avformat_find_stream_info (fctx, 0);
	if (r < 0)
	  {
	    break;
	  }

	r = av_find_best_stream (fctx, AVMEDIA_TYPE_AUDIO, -1, -1, &d, 0);

	if (r < 0)
	  {
	    break;
	  }
	stream = r;

	dec = avcodec_alloc_context3 (d);
	do
	  {
	    if (!dec)
	      {
		throw 1;
	      }

	    do {
	    avcodec_parameters_to_context (dec,
					   fctx->streams[stream]->codecpar);

	    r = avcodec_open2 (dec, d, NULL);
	    if (r < 0)
	      break;
	    return;
	    } while (0);
	  }
	while (0);
	avcodec_free_context (&dec);
      }
    while (0);
    avformat_close_input (&fctx);
    throw FrissonInputError ();
  }
  ~FrissonInput ()
  {
	  avcodec_free_context (&dec);
    avformat_close_input (&fctx);
  }
};

struct Encoder
{
  AVCodecContext *enc;
    Encoder (AVCodecID id = AV_CODEC_ID_PCM_S16LE)
  {
    AVCodec *e = avcodec_find_encoder (id);
    if (!e)
      throw EncoderError ();
      enc = avcodec_alloc_context3 (e);
      if (!enc)
	      throw 1;
      enc->sample_rate = 44100;
      enc->sample_fmt = e->sample_fmts[0];
      enc->channel_layout = AV_CH_LAYOUT_STEREO;
      enc->channels = 2;
    if (avcodec_open2 (enc, e, 0) < 0)
      throw EncoderError ();
  }

   ~Encoder ()
  {
    avcodec_free_context (&enc);
  }

};


struct FrissonDeckFilter
{
  AVFilterGraph *fg;
  AVFilterContext *sink, *src;

   ~FrissonDeckFilter ()
  {
    avfilter_graph_free (&fg);
  }

  FrissonDeckFilter (Encoder & encoder,
		     FrissonInput & input, string & filter_graph)
  {
    fg = avfilter_graph_alloc ();
    int r (0);
    AVCodecContext *ctx = input.dec;
    AVCodecContext *ectx = encoder.enc;
    if (!fg) throw 1;
    do
      {
	char buf[1054];
	const AVFilter *abufsrc = avfilter_get_by_name ("abuffer");
	const AVFilter *abufsink = avfilter_get_by_name ("abuffersink");


	AVRational tb = input.fctx->streams[input.stream]->time_base;

	if (!ctx->channel_layout)
	  {
	    ctx->channel_layout =
	      av_get_default_channel_layout (ctx->channels);
	  }
	snprintf (buf, 1054,
		  "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%llx",
		  tb.num, tb.den, ctx->sample_rate,
		  av_get_sample_fmt_name (ctx->sample_fmt),
		  ctx->channel_layout);

	r = avfilter_graph_create_filter (&src, abufsrc, "in", buf, 0, fg);

	if (r < 0)
	  {
	    break;
	  }

	r = avfilter_graph_create_filter (&sink, abufsink,
					  "out", NULL, NULL, fg);
	AVFilterInOut *outs = avfilter_inout_alloc ();
	AVFilterInOut *ins = avfilter_inout_alloc ();
	do
	  {
	    av_opt_set_bin (sink, "sample_rates",
			    (uint8_t *) & ectx->sample_rate,
			    sizeof (ctx->sample_rate),
			    AV_OPT_SEARCH_CHILDREN);
	    av_opt_set_bin (sink, "channel_layouts",
			    (uint8_t *) & ectx->channel_layout,
			    sizeof (ctx->channel_layout),
			    AV_OPT_SEARCH_CHILDREN);
	    av_opt_set_bin (sink, "sample_fmts",
			    (uint8_t *) & ectx->sample_fmt,
			    sizeof (ctx->sample_fmt), AV_OPT_SEARCH_CHILDREN);

	    outs->name = av_strdup ("in");
	    outs->filter_ctx = src;
	    outs->pad_idx = 0;
	    outs->next = 0;

	    ins->name = av_strdup ("out");
	    ins->filter_ctx = sink;
	    ins->pad_idx = 0;
	    ins->next = 0;

	    {
	      r =
		avfilter_graph_parse_ptr (fg, filter_graph.c_str (), &ins,
					  &outs, 0);
	      avfilter_inout_free (&ins);
	      avfilter_inout_free (&outs);
	    }
	    if (r < 0)
	      {
		break;
	      }
	    r = avfilter_graph_config (fg, NULL);
	    if (r < 0)
	      {
		throw FilterError ();
	      }
	    return;
	  }
	while (0);
	avfilter_inout_free (&ins);
	avfilter_inout_free (&outs);
      }
    while (0);
    avfilter_graph_free (&fg);

    throw FilterError ();
  }
};

struct MainDeckFilter
{
  AVFilterGraph *fg;
  AVFilterContext *sink, *src, *src1;
  pthread_mutex_t mutex;
  Encoder *enc;

   ~MainDeckFilter ()
  {
    avfilter_graph_free (&fg);
  }

  bool _reconfig (string filter_graph)
  {
    fg = avfilter_graph_alloc ();

    if (!fg) throw 1;
    int r (0);
    AVCodecContext *ctx = enc->enc;
    AVCodecContext *ectx = enc->enc;
    do
      {
	char buf[1054];
	const AVFilter *abufsrc = avfilter_get_by_name ("abuffer");
	const AVFilter *abufsink = avfilter_get_by_name ("abuffersink");


	AVRational tb = ctx->time_base;

	if (!ctx->channel_layout)
	  {
	    ctx->channel_layout =
	      av_get_default_channel_layout (ctx->channels);
	  }
	snprintf (buf, 1054,
		  "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%llx",
		  tb.num, tb.den, ctx->sample_rate,
		  av_get_sample_fmt_name (ctx->sample_fmt),
		  ctx->channel_layout);

	r = avfilter_graph_create_filter (&src, abufsrc, "in:0", buf, 0, fg);

	if (r < 0)
	  {
	    break;
	  }

	r = avfilter_graph_create_filter (&src1, abufsrc, "in:1", buf, 0, fg);

	if (r < 0)
	  {
	    break;
	  }

	r = avfilter_graph_create_filter (&sink, abufsink,
					  "out", NULL, NULL, fg);
	AVFilterInOut *outs = avfilter_inout_alloc ();
	AVFilterInOut *outs1 = avfilter_inout_alloc ();
	AVFilterInOut *ins = avfilter_inout_alloc ();
	do
	  {
	    av_opt_set_bin (sink, "sample_rates",
			    (uint8_t *) & ectx->sample_rate,
			    sizeof (ctx->sample_rate),
			    AV_OPT_SEARCH_CHILDREN);
	    av_opt_set_bin (sink, "channel_layouts",
			    (uint8_t *) & ectx->channel_layout,
			    sizeof (ctx->channel_layout),
			    AV_OPT_SEARCH_CHILDREN);
	    av_opt_set_bin (sink, "sample_fmts",
			    (uint8_t *) & ectx->sample_fmt,
			    sizeof (ctx->sample_fmt), AV_OPT_SEARCH_CHILDREN);

	    outs->name = av_strdup ("in:0");
	    outs->filter_ctx = src;
	    outs->pad_idx = 0;
	    outs->next = outs1;

	    outs1->name = av_strdup ("in:1");
	    outs1->filter_ctx = src1;
	    outs1->pad_idx = 0;
	    outs1->next = 0;

	    ins->name = av_strdup ("out");
	    ins->filter_ctx = sink;
	    ins->pad_idx = 0;
	    ins->next = 0;

	    {
	      r =
		avfilter_graph_parse_ptr (fg, filter_graph.c_str (), &ins,
					  &outs, 0);
	      avfilter_inout_free (&ins);
	      avfilter_inout_free (&outs);
	    }
	    if (r < 0)
	      {
		break;
	      }
	    r = avfilter_graph_config (fg, NULL);
	    if (r < 0)
	      {
		throw FilterError ();
	      }
	    return false;
	  }
	while (0);
	avfilter_inout_free (&ins);
	avfilter_inout_free (&outs);
      }
    while (0);
    avfilter_graph_free (&fg);

    throw FilterError ();
    return true;
  }

  bool reconfig (string str)
  {
    pthread_mutex_lock (&mutex);
    avfilter_graph_free (&fg);
    bool r (_reconfig (str));
    pthread_mutex_unlock (&mutex);

    return r;
  }

MainDeckFilter (Encoder & encoder, string filter_graph = "[in:0] [in:1] amerge [out]"):
	src (0),
	src1 (0),
	sink (0)
  {
    mutex = PTHREAD_MUTEX_INITIALIZER;
    fg = avfilter_graph_alloc ();
    enc = &encoder;

    int r (0);
    AVCodecContext *ctx = encoder.enc;
    AVCodecContext *ectx = encoder.enc;
    do
      {
	char buf[1054];
	const AVFilter *abufsrc = avfilter_get_by_name ("abuffer");
	const AVFilter *abufsink = avfilter_get_by_name ("abuffersink");


	AVRational tb = ctx->time_base;

	if (!ctx->channel_layout)
	  {
	    ctx->channel_layout =
	      av_get_default_channel_layout (ctx->channels);
	  }
	snprintf (buf, 1054,
		  "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%llx",
		  tb.num, tb.den, ctx->sample_rate,
		  av_get_sample_fmt_name (ctx->sample_fmt),
		  ctx->channel_layout);

	r = avfilter_graph_create_filter (&src, abufsrc, "in:0", buf, 0, fg);

	if (r < 0)
	  {
	    break;
	  }

	r = avfilter_graph_create_filter (&src1, abufsrc, "in:1", buf, 0, fg);

	if (r < 0)
	  {
	    break;
	  }

	r = avfilter_graph_create_filter (&sink, abufsink,
					  "out", NULL, NULL, fg);
	AVFilterInOut *outs = avfilter_inout_alloc ();
	AVFilterInOut *outs1 = avfilter_inout_alloc ();
	AVFilterInOut *ins = avfilter_inout_alloc ();
	do
	  {
	    av_opt_set_bin (sink, "sample_rates",
			    (uint8_t *) & ectx->sample_rate,
			    sizeof (ctx->sample_rate),
			    AV_OPT_SEARCH_CHILDREN);
	    av_opt_set_bin (sink, "channel_layouts",
			    (uint8_t *) & ectx->channel_layout,
			    sizeof (ctx->channel_layout),
			    AV_OPT_SEARCH_CHILDREN);
	    av_opt_set_bin (sink, "sample_fmts",
			    (uint8_t *) & ectx->sample_fmt,
			    sizeof (ctx->sample_fmt), AV_OPT_SEARCH_CHILDREN);

	    outs->name = av_strdup ("in:0");
	    outs->filter_ctx = src;
	    outs->pad_idx = 0;
	    outs->next = outs1;

	    outs1->name = av_strdup ("in:1");
	    outs1->filter_ctx = src1;
	    outs1->pad_idx = 0;
	    outs1->next = 0;

	    ins->name = av_strdup ("out");
	    ins->filter_ctx = sink;
	    ins->pad_idx = 0;
	    ins->next = 0;

	    {
	      r =
		avfilter_graph_parse_ptr (fg, filter_graph.c_str (), &ins,
					  &outs, 0);
	      avfilter_inout_free (&ins);
	      avfilter_inout_free (&outs);
	    }
	    if (r < 0)
	      {
		break;
	      }
	    r = avfilter_graph_config (fg, NULL);
	    if (r < 0)
	      {
		throw FilterError ();
	      }
	    return;
	  }
	while (0);
	avfilter_inout_free (&ins);
	avfilter_inout_free (&outs);
      }
    while (0);
    avfilter_graph_free (&fg);

    throw FilterError ();
  }
};;

struct FrissonDeck
{
  FrissonInput *input;
  Encoder *encoder;
  string fg;
  FrissonDeckFilter *din;
  MainDeckFilter *main_fd;
  int id;

   ~FrissonDeck ()
  {
    delete input;
    delete din;
  }

  FrissonDeck () = default;

  FrissonDeck (string track,
	       Encoder & enc, MainDeckFilter * mfd, string fg = "afifo")
  {
    try
    {
      input = new FrissonInput (track);
      encoder = &enc;

      din = new FrissonDeckFilter (*encoder, *input, fg);
      main_fd = mfd;
    }
    catch (FrissonInputError & err)
    {
      throw FrissonDeckError ();
      return;
    }
    catch (FilterError & err)
    {
      throw FrissonDeckError ();
      delete input;
    }
  }
};

struct Tracklist:public vector < string > {
  Tracklist (){};
  void add (char *str)
  {
    vector < string >::push_back (string (str));
  }
  string get_random ()
  {
    return vector <
      string >::operator[](Random (0, this->end () - this->begin ()));
  }
};

void
_deck_finalise (FrissonDeck & in)
{
  AVPacket *pkt = av_packet_alloc ();
  int r (0);
  AVCodecContext *enc = in.encoder->enc;

  while (1)
    {
      r = avcodec_receive_packet (enc, pkt);
      if (r < 0)
	break;

      if (pkt->data)
      {
	      fwrite (pkt->data, pkt->size, 1, stdout), fflush (stdout);
	      av_packet_unref (pkt);
      }
    }
  av_packet_free (&pkt);
}

void
deck_finalize (FrissonDeck & in)
{
  int r (0);
  AVFrame *frame = av_frame_alloc ();
  AVCodecContext *enc = in.encoder->enc;
  for (;; )
    {
	    try {
		do {
		      r = av_buffersink_get_frame_flags (in.main_fd->sink, frame, 4);
		      if (r < 0)
			throw r;

		      r = avcodec_send_frame (enc, frame);

		      if (r < 0)
			throw r;
		} while (0);
	    }catch (int &i){
		    break;
	    };

      _deck_finalise (in);
    }
  av_frame_free (&frame);
};

void
deck_receive_frames_sink (FrissonDeck & in)
{
  AVFrame *frame = av_frame_alloc ();
  int r (0);

  for (;;)
    {
	      pthread_mutex_lock (&in.main_fd->mutex);
	    try {
	      r = av_buffersink_get_frame (in.din->sink, frame);
	      if (r < 0)
	      {
		      throw r;
	      }
	      switch (in.id)
		{
		case 0:
		  r = av_buffersrc_add_frame (in.main_fd->src, frame);
		  break;
		default:
		  r = av_buffersrc_add_frame (in.main_fd->src1, frame);
		  break;
		};
	      if (r < 0)
	      {
		      throw r;
	      }
	      deck_finalize (in);
	    } catch (int &i)
	    {
		pthread_mutex_unlock (&in.main_fd->mutex);
		break;
	    }
	pthread_mutex_unlock (&in.main_fd->mutex);
    }
  av_frame_free (&frame);
}

void
deck_receive_frames (FrissonDeck & in)
{
  AVFrame *frame = av_frame_alloc ();
  int r (0);

  for (;;)
    {
      r = avcodec_receive_frame (in.input->dec, frame);

      if (r < 0)
	break;

      r = av_buffersrc_add_frame (in.din->src, frame);

      if (r < 0)
      {
	break;
      }
      deck_receive_frames_sink (in);
    }

  av_frame_free (&frame);
}

void
_deck_mixer (FrissonDeck & in)
{

  AVPacket *pkt = av_packet_alloc ();
  for (;;)
    {
      int r (av_read_frame (in.input->fctx, pkt));
      if (r < 0)
	break;
      if (pkt->stream_index != in.input->stream)
      {
	      av_packet_unref (pkt);
	continue;
      }
      //in.input->duration += pkt->duration;
      r = avcodec_send_packet (in.input->dec, pkt);
      if (r < 0)
      {
	continue;
      }
      av_packet_unref (pkt);
      deck_receive_frames (in);
    }

  av_packet_free (&pkt);
}

void
deck_mixer (Tracklist tracks, Encoder * encoder, MainDeckFilter * mdf, int id)
{
  while (true)
    {
      try
      {

	PyObject *obj;
	char *buf;
	string track=tracks.get_random ();

	pthread_mutex_lock (&pyU);
	obj = PyObject_CallMethod (PyImport_ImportModule ("__main__"),
				   "_deck_arg", "(is)", id,
				   track.c_str ());
	if(PyUnicode_Check (obj))
	{
		buf=(char*)PyUnicode_AsUTF8 (obj);
	}else buf="";
	Py_XDECREF (obj);
	pthread_mutex_unlock (&pyU);

	if ((string(buf))=="")
	{
		buf="afifo";
	}

	string single_desc (buf);
	FrissonDeck in = FrissonDeck (track, *encoder, mdf,
				      single_desc);
	in.id = id;

	pthread_mutex_lock (&pyU);
	PyObject_CallMethod (PyImport_ImportModule ("__main__"),
			     "req", "(is)", in.id,
			     in.input->filename.c_str ());
	pthread_mutex_unlock (&pyU);

	_deck_mixer (in);
      } catch (FrissonDeckError & fde)
      {
	continue;
      }
    }
}

MainDeckFilter *dp;

static PyObject *
Frisson_mix (PyObject * s, PyObject * a)
{
  char *str;
  if (!PyArg_ParseTuple (a, "s", &str))
    return 0;

  dp->reconfig (string (str));
  Py_XINCREF (Py_None);
  return Py_None;
}

int
main (int argsc, char **args)
{

  srand (time (0));
  av_log_set_callback (0);
  Py_InitializeEx (0);

  PyMethodDef methods[] = {
    {"mix", Frisson_mix, METH_VARARGS, ""},
    {NULL}
  };
  PyModule_AddFunctions (PyImport_ImportModule ("builtins"), methods);

  FILE *fp = fopen ("mixxpy.py", "r");
  if (!fp)
    return 1;
  PyRun_AnyFileEx (fp, "mixxpy.py", 1);

  Encoder enc = Encoder ();
  MainDeckFilter mdf = MainDeckFilter (enc);
  dp = &mdf;

  try
  {
    Tracklist list;
    for (int i = 1; i < argsc; i++)
      {
	list.add (args[i]);
      }
    thread ft (deck_mixer, list, &enc, &mdf, 1);
    thread ft1 (deck_mixer, list, &enc, &mdf, 0);

    ft.join ();
  } catch (FrissonDeckError & error)
  {
    cout << "Failed: To open Deck" << endl;
  }

  Py_Finalize ();
  return (0);
}
