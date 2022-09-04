#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <assert.h>

int main ( signed Argsc, char *(Args[]) )
{
	AVFilterGraph *fg = avfilter_graph_alloc ();
	AVCodec *e = avcodec_find_encoder (AV_CODEC_ID_MP3);

	AVFilterContext *out, *out1, *sine, *asplit, *bass1;

	AVCodecContext *enc = avcodec_alloc_context3 (e);

	enc->sample_fmt = e->sample_fmts[0];
	enc->sample_rate = 44100;
	enc->channels = 2;
	enc->channel_layout = AV_CH_LAYOUT_STEREO;

	assert (avcodec_open2 (enc, e, 0)>=0);

	assert ((sine = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("sine"), NULL))>=0);
	assert ((asplit = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("asplit"), NULL))>=0);
	assert ((out = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("abuffersink"), NULL))>=0);
	assert ((out1 = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("abuffersink"), NULL))>=0);
	assert ((bass1 = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("aecho"), NULL))>=0);

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

	assert (avfilter_init_str (sine, NULL)>=0);
	assert (avfilter_init_str (out, NULL)>=0);
	assert (avfilter_init_str (out1, NULL)>=0);
	assert (avfilter_init_str (bass1, NULL)>=0);
	assert (avfilter_init_str (asplit, NULL)>=0);

	assert (avfilter_link (sine, 0, asplit, 0)>=0);
	assert (avfilter_link (asplit, 0, bass1, 0)>=0);
	assert (avfilter_link (bass1, 0, out, 0)>=0);
	assert (avfilter_link (asplit, 1, out1, 0)>=0);

	assert (avfilter_graph_config (fg, NULL)>=0);

	AVFrame *frame=av_frame_alloc ();

	AVFilterContext *oops[2]={
		out, out1
	};
	int x=0;

	AVPacket *pkt = av_packet_alloc ();
	av_log_set_callback (0);

	while (1)
	{
		int r=av_buffersink_get_frame (oops[x=!x], frame);

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

		av_packet_unref (pkt);
	}

	return 0;
}

