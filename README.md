This program demonstrates (what is believed to be) a memory leak associated with freeing RtpMuxContexts.
 
The program reads a media source, specified on the command line, and streams the packets from that source onto a multicast address as an RTP stream.

The program is pretty mundane and the program works as expected.  Just another day in FFMPEG land.

However, the program leaks memory unless special steps are taken.

The lines of interest are at the end of the program; <https://github.com/trafficland/rtpmemleak/blob/master/src/main.cpp#L147> through <https://github.com/trafficland/rtpmemleak/blob/master/src/main.cpp#L154>.

If one comments out lines 149 and 150 and runs valgrind (valgrind --leak-check=yes ./bin/rtpmemleak path_to_source) the following report will be generated.

<pre><code>
==17351== 
==17351== HEAP SUMMARY:
==17351==     in use at exit: 1,512 bytes in 2 blocks
==17351==   total heap usage: 204 allocs, 203 frees, 613,993 bytes allocated
==17351== 
==17351== 1,472 bytes in 1 blocks are definitely lost in loss record 2 of 2
==17351==    at 0x4C26588: memalign (vg_replace_malloc.c:727)
==17351==    by 0x4C26623: posix_memalign (vg_replace_malloc.c:876)
==17351==    by 0xC38559: av_malloc (mem.c:95)
==17351==    by 0x57AABC: rtp_write_header (rtpenc.c:149)
==17351==    by 0x53701B: avformat_write_header (mux.c:406)
==17351==    by 0x49C9E0: main (in /media/psf/Home/dev/rtp_mem_leak/bin/rtpmemleak)
==17351== 
==17351== LEAK SUMMARY:
==17351==    definitely lost: 1,472 bytes in 1 blocks
==17351==    indirectly lost: 0 bytes in 0 blocks
==17351==      possibly lost: 0 bytes in 0 blocks
==17351==    still reachable: 40 bytes in 1 blocks
==17351==         suppressed: 0 bytes in 0 blocks
==17351== Reachable blocks (those to which a pointer was found) are not shown.
==17351== To see them, rerun with: --leak-check=full --show-reachable=yes
==17351== 
==17351== For counts of detected and suppressed errors, rerun with: -v
==17351== ERROR SUMMARY: 2 errors from 2 contexts (suppressed: 6 from 6)
</code></pre>

If lines 149 and 150 remain active the following report is generated.

<pre><code>
==17391== 
==17391== HEAP SUMMARY:
==17391==     in use at exit: 40 bytes in 1 blocks
==17391==   total heap usage: 204 allocs, 203 frees, 613,993 bytes allocated
==17391== 
==17391== LEAK SUMMARY:
==17391==    definitely lost: 0 bytes in 0 blocks
==17391==    indirectly lost: 0 bytes in 0 blocks
==17391==      possibly lost: 0 bytes in 0 blocks
==17391==    still reachable: 40 bytes in 1 blocks
==17391==         suppressed: 0 bytes in 0 blocks
==17391== Reachable blocks (those to which a pointer was found) are not shown.
==17391== To see them, rerun with: --leak-check=full --show-reachable=yes
==17391== 
==17391== For counts of detected and suppressed errors, rerun with: -v
==17391== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 6 from 6)
</code></pre>

There is clearly a leak.  I'm trying to determine if it's a bug in our code or libavformat.
 
Through debugging I have found the following.
 
The memory is allocated by avformat_write_header <https://github.com/trafficland/rtpmemleak/blob/master/src/main.cpp#L119>.

That call is eventually passed to an implementation specific to RTP, rtp_write_header <https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/rtpenc.c#L89>.

At that point the AVFormatContext priv_data has been assigned a RtpMuxContext <https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/rtpenc.h#L27> which has a buf field <https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/rtpenc.h#L49>.

AVFormatContext priv_data is cleaned up by avformat_free_context <https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/utils.c#L3608> which is called by avformat_close_input <https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/rtpenc.h#L49>.

Since RtpMuxContext is an AVOptions-enabled struct its fields should be cleaned up by a call to av_opt_free <https://github.com/FFmpeg/FFmpeg/blob/master/libavutil/opt.c#L1434>.

While stepping through the code it was determined that the calls to av_opt_next <https://github.com/FFmpeg/FFmpeg/blob/master/libavutil/opt.c#L1437> never returns the buf field to be cleaned up.