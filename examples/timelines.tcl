source "stdlib.tcl"

set sr 44100
set samps [* 30 $sr]

set tab1 [gen 4096 1 0.5 0.25 0.125]
set tab2 [gen 4096 1]

set amps [list [bpf 0 [/ $samps 3] 0.3 [/ $samps 3] 0.3 [/ $samps 3] 0] [bpf 0 [/ $samps 2] 0.3 [/ $samps 2] 0] [bpf 0 [/ $samps 2] 0.3 [/ $samps 2] 0]]
set freqs [list [bpf 220 $samps 550] [bpf 110 $samps 220] [bpf 440 $samps 220]]

set env [bpf 0 [/ $samps 2] 0.5 [/ $samps 2] 0]

set s1 0 #[* $env [osc $sr [bpf 220 $samps 330] $tab1]]
set s2 0 #[* $env [osc $sr [bpf 220 $samps 1660] $tab1]]
set s3 [oscbank $sr $amps $freqs $tab2]
set s4 0 #[* $env [osc $sr [bpf 110 $samps 60] $tab1]]

set timeline1 [mix 0 $s1 [/ $samps 2] $s2 [* 3 [/ $samps 4]] $s3 0 $s4]]

set irL [car [cdr [sndread "../sounds/Concertgebouw-s.wav"]]]
set irR [car [cdr [cdr [sndread "../sounds/Concertgebouw-s.wav"]]]]

set scale 0.01
set mix 0.4

set outsigL [conv $irL $timeline1 $scale $mix]
set outsigR [conv $irR $timeline1 $scale $mix]

sndwrite $sr "timelines.wav" $outsigL $outsigR
