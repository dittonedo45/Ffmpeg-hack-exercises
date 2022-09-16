#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <assert.h>

/* 
  time_base         <rational>   ..F.A...... (from 0 to INT_MAX) (default 0/1)
  sample_rate       <int>        ..F.A...... (from 0 to INT_MAX) (default 0)
  sample_fmt        <sample_fmt> ..F.A...... (default none)
  channel_layout    <string>     ..F.A......
  channels          <int>        ..F.A...... (from 0 to INT_MAX) (default 0)

   */

int
main ( signed Argsc, char *(Args[]) )
{
	int r;
	AVFilterGraph *fg = avfilter_graph_alloc ();
	AVCodec *e = avcodec_find_encoder (AV_CODEC_ID_MP3);

	AVFilterContext *out, *out1, *sine, *asplit, *bass1;

	AVCodecContext *enc = avcodec_alloc_context3 (e);

	AVCodec *d = NULL;
	AVCodecContext *dec;
	AVFormatContext *fctx=0;
	int stream;
	
	r = avformat_open_input (&fctx, Args[1], NULL, NULL);

    if (r < 0)
      {
	      abort ();
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
	if (!dec)
	  {
	    break;
	  }
	    do
	      {
		avcodec_parameters_to_context (dec,
					       fctx->streams[stream]->
					       codecpar);

		r = avcodec_open2 (dec, d, NULL);
		if (r < 0)
		  break;
		goto pip;
	      }
	    while (0);
	avcodec_free_context (&dec);
      }
    while (0);
    avformat_close_input (&fctx);
    abort ();
pip:
    if (!dec->channel_layout)
        dec->channel_layout = av_get_default_channel_layout(dec->channels);

	enc->sample_fmt = e->sample_fmts[0];
	enc->sample_rate = 44100;
	enc->channels = 2;
	enc->channel_layout = AV_CH_LAYOUT_STEREO;

	assert (avcodec_open2 (enc, e, 0)>=0);

	assert ((sine = avfilter_graph_alloc_filter (fg,
		avfilter_get_by_name ("abuffer"), NULL))>=0);
	av_opt_set_q (sine, "time_base", dec->time_base, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int (sine, "sample_rate", dec->sample_rate, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int (sine, "sample_fmt", dec->sample_fmt, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int (sine, "channels", av_get_channel_layout_nb_channels (dec->channel_layout),
			AV_OPT_SEARCH_CHILDREN);
	{
		char ch[64];
		av_get_channel_layout_string(ch, sizeof(ch), av_get_channel_layout_nb_channels (dec->channel_layout),
				dec->channel_layout);
		
		av_opt_set (sine, "channel_layout", ch, AV_OPT_SEARCH_CHILDREN);
	}

	assert ((asplit = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("asplit"), NULL))>=0);
	assert ((out = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("abuffersink"), NULL))>=0);
	assert ((out1 = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("abuffersink"), NULL))>=0);
	assert ((bass1 = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("afifo"), NULL))>=0);

	av_opt_set_bin (out, "sample_fmts", &enc->sample_fmt,
			sizeof(enc->sample_fmt), AV_OPT_SEARCH_CHILDREN );

	av_opt_set_bin (out, "sample_rates", &enc->sample_rate,
			sizeof(enc->sample_rate), AV_OPT_SEARCH_CHILDREN );

	av_opt_set_bin (out, "channel_layouts", &enc->channel_layout,
			sizeof(enc->channel_layout), AV_OPT_SEARCH_CHILDREN );

	av_opt_set_bin (out1, "sample_fmts", &enc->sample_fmt,
			sizeof(enc->sample_fmt), AV_OPT_SEARCH_CHILDREN );
	av_opt_set_bin (out1, "sample_rates", &enc->sample_rate,
			sizeof(enc->sample_rate), AV_OPT_SEARCH_CHILDREN );

	av_opt_set_bin (out1, "channel_layouts", &enc->channel_layout,
			sizeof(enc->channel_layout), AV_OPT_SEARCH_CHILDREN );

	/*
	av_opt_set_double (bass1, "in_gain", 0.88889, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_double (bass1, "out_gain", 0.48889, AV_OPT_SEARCH_CHILDREN);
	av_opt_set (bass1, "delays", "5", AV_OPT_SEARCH_CHILDREN);
	av_opt_set (bass1, "decays", "0.9", AV_OPT_SEARCH_CHILDREN);
	*/


	assert (avfilter_init_str (sine, NULL)>=0);

	assert (avfilter_init_str (out, NULL)>=0);
	assert (avfilter_init_str (out1, NULL)>=0);
	assert (avfilter_init_str (bass1, NULL)>=0);
	assert (avfilter_init_str (asplit, NULL)>=0);

	assert (avfilter_link (sine, 0, asplit, 0)>=0);
	assert (avfilter_link (asplit, 0, bass1, 0)>=0);
	{
		AVFilterInOut *in = avfilter_inout_alloc ();
		AVFilterInOut *oom = avfilter_inout_alloc ();

		oom->name = av_strdup ("out");
		in->name =av_strdup ("in");

		oom->filter_ctx = out;
		oom->pad_idx = 0;
		in->filter_ctx = bass1;
		in->pad_idx = 0;

		assert (avfilter_graph_parse (fg, "[in]afifo[out]", oom, in, NULL)>=0);

		in = avfilter_inout_alloc ();
		oom = avfilter_inout_alloc ();

		oom->name = av_strdup ("out");
		oom->filter_ctx = out1;
		oom->pad_idx = 0;
		oom->next = 0;

		in->name =av_strdup ("in");
		in->filter_ctx = asplit;
		in->pad_idx = 1;
		in->next = 0;

		assert (avfilter_graph_parse (fg, "[in]afifo[out]", oom, in, NULL)>=0);

	}


	assert(avfilter_graph_config (fg, NULL)>=0);

	AVFrame *frame=av_frame_alloc ();

	AVFilterContext *oops[2]={
		out, out1
	};
	int x=0;

	AVPacket *pkt = av_packet_alloc ();
	av_log_set_callback (0);

	while (1)
	{
		int r=0;

		r = av_read_frame (fctx, pkt);

		if (r<0)
			break;
		if (pkt->stream_index != stream )
		{
			continue;
		}

		r = avcodec_send_packet (dec, pkt);

		if (r < 0 ) continue;

		r = avcodec_receive_frame (dec, frame);
		
		if (r < 0 ) continue;

		r = av_buffersrc_add_frame (sine, frame);

		if (r < 0 ) continue;

		r = av_buffersink_get_frame (oops[x=!x], frame);

		if (r<0)
		{
			break;
		}

		r = avcodec_send_frame (enc, frame);
		av_frame_unref (frame);
		if (r<0)
		{
			continue;
		}
		r=avcodec_receive_packet (enc, pkt);
		if (r<0)
		{
			continue;
		}
		fwrite (pkt->data, pkt->size, 1, stdout);
		av_packet_unref (pkt);
	}
	return 0;
}

