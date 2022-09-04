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
	AVFilterContext *out, *sine;

	AVCodecContext *enc = avcodec_alloc_context3 (e);

	enc->sample_fmt = e->sample_fmts[0];
	enc->sample_rate = 44100;
	enc->channels = 2;
	enc->channel_layout = AV_CH_LAYOUT_STEREO;

	assert (avcodec_open2 (enc, e, 0)>=0);

	assert ((sine = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("sine"), NULL))>=0);

	assert ((out = avfilter_graph_alloc_filter (fg, avfilter_get_by_name ("abuffersink"), NULL))>=0);

	av_opt_set_bin (out, "sample_fmts", &enc->sample_fmt,
			sizeof(enc->sample_fmt), AV_OPT_SEARCH_CHILDREN );

	av_opt_set_bin (out, "sample_rates", &enc->sample_rate,
			sizeof(enc->sample_rate), AV_OPT_SEARCH_CHILDREN );

	av_opt_set_bin (out, "sample_fmts", &enc->channel_layout,
			sizeof(enc->channel_layout), AV_OPT_SEARCH_CHILDREN );

	assert (avfilter_init_str (sine, NULL)>=0);
	assert (avfilter_init_str (out, NULL)>=0);

	assert (avfilter_link (sine, 0, out, 0)>=0);

	assert (avfilter_graph_config (fg, NULL)>=0);

	AVFrame *frame=av_frame_alloc ();

	while (1)
	{
		int r=av_buffersink_get_frame (out, frame);

		if (r<0) break;

		puts ("---");
		av_frame_unref (frame);

		sleep (1);
	}

	return 0;
}

