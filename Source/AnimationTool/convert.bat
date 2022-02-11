del animation.gif
del animation.mp4

rem brightness/gamma, source directory, destination file
skanim f 1.5 1.55 c:\temp animation.zap

rem generate a preview

rem on linux use -i out%04d.png (no double '%')
ffmpeg -f image2 -pix_fmt gray -framerate 25 -pattern_type sequence -start_number 0 -s 192x147 -i "out%%04d.raw" animation.mp4
ffmpeg -i animation.mp4 -vf vflip -pix_fmt gray -s 192x147 animation.gif

del *.raw
del animation.mp4
