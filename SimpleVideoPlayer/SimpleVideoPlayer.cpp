#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "SimpleVideoPlayer.h"

int main(int argc, char *argv[])
{
	// Initialization Variables
	int				 i, videoStream;
	AVFormatContext *pFormatCtx		= NULL;
	AVCodecContext	*pCodecCtxOrig	= NULL;
	AVCodecContext	*pCodecCtx		= NULL;
	AVCodec			*pCodec			= NULL;
	AVFrame			*pFrame			= NULL;
	AVFrame			*pFrameRGB		= NULL;
	uint8_t			*buffer			= NULL;
	int				 numBytes;

	// Reading the Data Variables
	struct SwsContext	*sws_ctx = NULL;
	int					 frameFinished;
	AVPacket			 packet;

	// Register all file format and codecs, it will autodetect. 
	// Specific file format or codec can also be done 
	av_register_all();

	// Open video file 
	if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
		return -1;

	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		return -1;

	// Dumo file information to stderr
	av_dump_format(pFormatCtx, 0, argv[1], 0);

	// Find first video stream
	videoStream = -1;
	for (i = 0; i < (int)pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoStream = i;
			break;
		}
	}

	if (videoStream == -1)
		return -1;

	// Get a pointer to codec context for the video stream 
	pCodecCtx = pFormatCtx->streams[videoStream]->codec;

	// Find a decoder for the video stream 
	pCodec = avcodec_find_decoder(pCodecCtx -> codec_id);
	if (pCodec == NULL)
	{
		fprintf(stderr, "Unsupported codec!\n");
		return -1;
	}

	// Copy context
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0)
	{
		fprintf(stderr, "Couldn't copy codec context\n");
		return -1;
	}

	// Open Codec 
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
		return -1;
	
	// Allocate video frame 
	pFrame = av_frame_alloc();

	// Allocate an AVFrame structure
	pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL)
		return -1;
	
	// Determine required buffer size and allocate buffer
	numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

	buffer = (uint8_t *)av_malloc(numBytes *sizeof(uint8_t));


	// Mapping of our buffer to the image plane.
	// AVFrame is supeset of AVPictue 
	avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);


	// Read the stream, then decode it to our frame and save it

	// Init Sws context for software scaling 
	sws_ctx = sws_getContext(pCodecCtx -> width, 
							 pCodecCtx -> height, 
							 pCodecCtx -> pix_fmt, 
							 pCodecCtx -> width, 
							 pCodecCtx -> height,
							 AV_PIX_FMT_RGB24, 
							 SWS_BILINEAR,
							 NULL, 
							 NULL, 
							 NULL );
	i = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0)
	{
		// If this is video stream packet 
		if (packet.stream_index == videoStream)
		{
			// Decode video frame 
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

			// If we got a video frame 
			if (frameFinished)
			{
				// Convert from native to RGB format
				sws_scale(sws_ctx, 
						  (uint8_t const * const *)pFrame->data,
						  pFrame -> linesize, 
						  0, 
						  pCodecCtx -> height, 
						  pFrameRGB -> data, 
						  pFrameRGB -> linesize);

				// Saving the frame to disk
				if (++i <= 5)
					SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
			}
		}

		// Free the packet allocated by av_read_frame
		av_free_packet(&packet);
	}

	// Clean up

	// Free the RGB image 
	if (buffer)
	{
		av_free(buffer);
		buffer = NULL;
	}

	if (pFrameRGB)
	{
		av_free(pFrameRGB);
		pFrameRGB = NULL;
	}

	// Free the YUV frame 
	if (pFrame)
	{
		av_free(pFrame);
		pFrame = NULL;
	}

	// Close the codecs 
	if (pCodecCtx)
	{
		avcodec_close(pCodecCtx);
		pCodecCtx = NULL;
	}

	if (pCodecCtxOrig)
	{
		avcodec_close(pCodecCtxOrig);
		pCodecCtxOrig = NULL;
	}

	// Close the video file 
	if (pFormatCtx)
	{
		avformat_close_input(&pFormatCtx);
		pFormatCtx = NULL;
	}

	return 0;
}
	
void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)
{
	FILE *pFile;
	char szFileName[32];
	int y;

	// Open file 
	sprintf(szFileName,	"frame%d.ppm", iFrame);
	pFile = fopen(szFileName, "wb");
	if (pFile == NULL)
		return;

	// Write header 
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data 
	for (y = 0; y < height; y++)
		fwrite(pFrame->data[0] + y * pFrame->linesize[0],	1,	width * 3, pFile);

	// Close file 
	fclose(pFile);
}