#include <iostream>
#include <fstream>
#include "tl_utils.h"

extern "C" {
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
}

//Had to copy this into the project so that I could cast dstFmtCtxt->priv_data to a RTPMuxContext
struct RTPMuxContext {
    const AVClass *av_class;
    AVFormatContext *ic;
    AVStream *st;
    int payload_type;
    uint32_t ssrc;
    const char *cname;
    int seq;
    uint32_t timestamp;
    uint32_t base_timestamp;
    uint32_t cur_timestamp;
    int max_payload_size;
    int num_frames;

    /* rtcp sender statistics */
    int64_t last_rtcp_ntp_time;
    int64_t first_rtcp_ntp_time;
    unsigned int packet_count;
    unsigned int octet_count;
    unsigned int last_octet_count;
    int first_packet;
    /* buffer for output */
    uint8_t *buf;
    uint8_t *buf_ptr;

    int max_frames_per_packet;

    /**
     * Number of bytes used for H.264 NAL length, if the MP4 syntax is used
     * (1, 2 or 4)
     */
    int nal_length_size;

    int flags;

    unsigned int frame_count;
};

typedef struct RTPMuxContext RTPMuxContext;

int main(int argc, char** argv) {
    const int FilenameArg = 1;
    const int NumberOfFrames = 2;
    const int SteamIndexOfInterest = 0;

    tl_utils::info("RTP MEM LEAK DEMO");
    av_log_set_level(AV_LOG_DEBUG);

    std::string filename;
    int numFramesRequested = 10;
    switch(argc){
        case 2:
            filename = argv[FilenameArg];
            break;
        case 3:
            filename = argv[FilenameArg];
            numFramesRequested = std::stoi(argv[NumberOfFrames]);
            break;
        default:
            tl_utils::info("USAGE: rtpmemleak <input file> [number of packets]");
            return EXIT_FAILURE;
    }

    tl_utils::debug("Opening the media file.");
    av_register_all();
    avformat_network_init();
    AVFormatContext* srcFmtCtxt = nullptr;
    if(avformat_open_input(&srcFmtCtxt, filename.c_str(), nullptr, nullptr) != 0){
        tl_utils::debug("Could not open " + filename + ".");
        return EXIT_FAILURE;
    }

    AVPacket avPacket;
    auto numFramesProcessed = 0;
    std::string outputAddr = "udp://239.0.0.1:6565";
    auto outputFmt = "RTP";
    AVFormatContext* dstFmtCtxt = nullptr;
    avformat_alloc_output_context2(&dstFmtCtxt, nullptr, outputFmt, outputAddr.c_str());
    auto outputFormat = dstFmtCtxt->oformat;
    AVStream* inputStream = srcFmtCtxt->streams[SteamIndexOfInterest];
    AVStream* outputStream = avformat_new_stream(dstFmtCtxt, inputStream->codec->codec);
    if (outputStream == nullptr) {
        tl_utils::debug("Failed allocating output stream");
        return EXIT_FAILURE;
    }

    auto ret = avcodec_copy_context(outputStream->codec, inputStream->codec);
    if (ret < 0) {
        tl_utils::debug("Failed to copy context from input to output stream codec context.");
        return EXIT_FAILURE;
    }

    outputStream->codec->codec_tag = 0;
    if (outputFormat->flags & AVFMT_GLOBALHEADER) {
        outputStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!(outputFormat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&dstFmtCtxt->pb, outputAddr.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            tl_utils::debug("Could not open output file " + outputAddr + ".");
            return EXIT_FAILURE;
        }
    }

    //*** avformat_write_header allocates memory (the underlying call is to rtp_write_header in rtpenc.c) and assigns it to dstFmtCtxt->priv_data->buf.
    //*** dstFmtCtxt->priv_data->buf is actually a RtpMuxContext which is an AVOptions-enabled struct.
    //*** The memory assigned to dstFmtCtxt->priv_data->buf is not cleaned up by avformat_close_input when avformat_close_input cleans up the AVOptions.
    ret = avformat_write_header(dstFmtCtxt, nullptr);
    if (ret < 0) {
        tl_utils::debug("Failed to write header.");
        return EXIT_FAILURE;
    }

    while (numFramesProcessed < numFramesRequested && av_read_frame(srcFmtCtxt, &avPacket) == 0) {
        tl_utils::debug("Starting packet.");
        if(avPacket.stream_index == SteamIndexOfInterest) {
            AVStream *in_stream, *out_stream;
            in_stream = srcFmtCtxt->streams[avPacket.stream_index];
            out_stream = dstFmtCtxt->streams[avPacket.stream_index];
            avPacket.pts = av_rescale_q_rnd(avPacket.pts, in_stream->time_base, out_stream->time_base,
                                            static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            avPacket.dts = av_rescale_q_rnd(avPacket.dts, in_stream->time_base, out_stream->time_base,
                                            static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            avPacket.duration = (int) av_rescale_q(avPacket.duration, in_stream->time_base, out_stream->time_base);
            avPacket.pos = -1;
            ret = av_interleaved_write_frame(dstFmtCtxt, &avPacket);
            if (ret < 0) {
                tl_utils::debug("Error muxing packet.");
            }
        }

        av_free_packet(&avPacket);
        tl_utils::debug("Finished packet " + std::to_string(numFramesProcessed++));
    }

    //*** The memory assigned to dstFmtCtxt->priv_data->buf must be cleaned up manually as it is not cleaned up by avformat_close_input
    //*** Comment out the next 2 lines and run valgrind to see the leak stats.
    RTPMuxContext* x = (struct RTPMuxContext *) dstFmtCtxt->priv_data;
    av_free(x->buf);

    tl_utils::debug("Closing dstFmtCtxt.");
    avformat_close_input(&dstFmtCtxt);
    tl_utils::debug("Closed dstFmtCtxt.");

    tl_utils::debug("Closing srcFmtCtxt.");
    avformat_close_input(&srcFmtCtxt);
    tl_utils::debug("Closed srcFmtCtxt.");

    return (EXIT_SUCCESS);
}