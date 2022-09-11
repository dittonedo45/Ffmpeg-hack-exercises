#include <python3.8/Python.h>
#include <pulse/simple.h>
#include <espeak-ng/speak_lib.h>
#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <assert.h>

pa_simple *simp;
AVCodecContext *dec, *enc;
AVFilterContext *src, *sink;

static void
_continue ()
{
	AVFrame *frame=av_frame_alloc ();
	AVPacket *pkt=av_packet_alloc ();

	while (1)
	{
		int r;
		r=av_buffersink_get_frame (sink, frame);
		if (r<0)
			break;
		r=avcodec_send_frame (enc, frame);
		if (r<0)break;
		r=avcodec_receive_packet (enc, pkt);
		if (r<0)
			break;
		else{
			fwrite (pkt->data, pkt->size, 1, stdout);
			av_packet_unref (pkt);
		}
	}
	av_packet_free (&pkt);
	av_frame_free (&frame);
}

static int
my_voice_box (short *opa, int ns, espeak_EVENT * ev)
{
  int sz, error = 0;
  int r;

  espeak_Synchronize ();
  if (ns==0) return 0;
  AVPacket pkt={0};

  av_packet_from_data (&pkt, (uint8_t *)opa, ns * 2);

  if ((r=avcodec_send_packet (dec, &pkt))<0)
  {
	  return 0;
  }

  AVFrame *frame=av_frame_alloc ();
  while (1)
  {
	  if (avcodec_receive_frame (dec, frame)<0)
	  {
		  break;
	  }
	  if (av_buffersrc_add_frame (src, frame)<0)
	  {
		  break;
	  }
	  av_frame_unref (frame);
	  _continue ();
  }
  av_frame_free (&frame);
  return 0;
}



int
main (int argsc, char **args)
{
  int r;
  int rate;
  {
    rate = r = espeak_Initialize (AUDIO_OUTPUT_RETRIEVAL, 1, NULL, 0);
    espeak_SetSynthCallback (my_voice_box);
  }
  AVCodec *d = avcodec_find_decoder (AV_CODEC_ID_PCM_S16LE);
  dec = avcodec_alloc_context3 (d);
  dec->sample_rate = rate;
  dec->sample_fmt = AV_SAMPLE_FMT_S16;
  dec->channels = 1;
  dec->channel_layout = AV_CH_LAYOUT_MONO;
  assert (avcodec_open2 (dec, d, 0) >= 0);


  {
	  AVCodec *e = avcodec_find_encoder (AV_CODEC_ID_MP3);
	  enc = avcodec_alloc_context3 (e);
	  enc->sample_rate = 44100;
	  enc->sample_fmt = e->sample_fmts[0];
	  enc->channels = 2;
	  enc->channel_layout = AV_CH_LAYOUT_STEREO;

	  assert (avcodec_open2 (enc, e, 0) >= 0);
  }

  AVFilterGraph *fg=avfilter_graph_alloc ();
  if (!fg)
	  abort ();
  char buf[1054];

  snprintf (buf, 1054,
	  "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%llx",
	  r,
	  dec->sample_rate,
	  av_get_sample_fmt_name (dec->sample_fmt),
	  dec->channel_layout);

  assert(avfilter_graph_create_filter (&src, avfilter_get_by_name ("abuffer"), 0, buf,
			  0, fg)>=0);

  assert(avfilter_graph_create_filter (&sink, avfilter_get_by_name ("abuffersink"), 0, 0,
			  0, fg)>=0);

    av_opt_set_bin (sink, "sample_rates",
		    (uint8_t *) & enc->sample_rate,
		    sizeof (enc->sample_rate),
		    AV_OPT_SEARCH_CHILDREN);
    av_opt_set_bin (sink, "channel_layouts",
		    (uint8_t *) & enc->channel_layout,
		    sizeof (enc->channel_layout),
		    AV_OPT_SEARCH_CHILDREN);
    av_opt_set_bin (sink, "sample_fmts",
		    (uint8_t *) & enc->sample_fmt,
		    sizeof (enc->sample_fmt), AV_OPT_SEARCH_CHILDREN);

    {
		AVFilterInOut *in = avfilter_inout_alloc ();
		AVFilterInOut *oom = avfilter_inout_alloc ();

		oom->name = av_strdup ("out");
		in->name = av_strdup ("in");

		oom->filter_ctx = sink;
		oom->pad_idx = 0;

		in->filter_ctx = src;
		in->pad_idx = 0;

		assert (avfilter_graph_parse (fg, "[in] asplit [a], lowshelf, [a] amix [out]", oom, in, NULL)>=0);
    }

    assert (avfilter_graph_config (fg, 0)>=0);

    av_log_set_callback (0);

    espeak_SetVoiceByName ("robert");
    for (char **p=args+1; p && *p; p++)
    {
	    const char *ans = *p;
	    int len=strlen (ans);
	  espeak_Synth (ans, 4, 0, POS_CHARACTER, len, espeakCHARS_UTF8, NULL,
					(void*) simp);
    }
  espeak_Synchronize ();
  return (0);
}
