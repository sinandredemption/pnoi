plughw=$(arecord -l | grep card | awk '{print int($2)}')
arecord -D plughw:$plughw -c2 -d 180 -r 16000 -f S32_LE -t wav -V stereo -v stereo_sample.wav

