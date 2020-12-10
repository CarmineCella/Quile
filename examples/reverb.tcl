source "stdlib.tcl"

set scale 0.1
set mix 0.8

set sig [car [cdr [sndread "../sounds/anechoic1.wav"]]]
set irL [car [cdr [sndread "../sounds/Concertgebouw-s.wav"]]]
set irR [car [cdr [cdr [sndread "../sounds/Concertgebouw-s.wav"]]]]

set outsigL [conv $irL $sig $scale $mix]
set outsigR [conv $irR $sig $scale $mix]

sndwrite 44100 "reverb.wav" $outsigL $outsigR

