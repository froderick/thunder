brew install ffmpeg
brew install mpv

ffmpeg -f avfoundation -list_devices true -i ""
ffmpeg -f avfoundation -video_size 320x240 -framerate 30 -i "HD Pro Webcam C920" video-only.mkv
mpv video-only.mkv

ffmpeg -f avfoundation -video_size 320x240 -framerate 30 -i "HD Pro Webcam C920" -c:v libx264 -preset ultrafast -f matroska - | mpv -
