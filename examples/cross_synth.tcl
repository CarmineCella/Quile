source "stdlib.tcl"

set sr 44100
set hop 512
set sz 2048

set sig1 [car [cdr [sndread "../sounds/Vox.wav"]]]
set sig2 [car [cdr [sndread "../sounds/Beethoven_Symph7.wav"]]]

set sig1_len [size $sig1]]
set sig2_len [size $sig2]]
set min_len [size $sig1]]

if {< $sig2_len $sig1_len} {set min_len $sig2_len}
set min_len [- $min_len $sz]

puts "sig 1 len = " [size $sig1] ", sig 2 len = " [size $sig2] ", min len = " $min_len $nl

set i 0
set bartlett [bpf 0 [/ $sz 2] 1 [/ $sz 2] 0]
set outsig [bpf 0 [+ $sz $min_len] 0]]

set threshold [bpf 0.0001 [/ $sz 2] 0.0001] # denoise

while {< $i $min_len} {
    set buff1 [slice $sig1 $i $sz]
    set buff1 [* $bartlett $buff1]
    set spec1 [car2pol [fft $buff1]]
    set amps1 [slice $spec1 0 [/ [size $spec1] 2] 2]
    set phi1  [slice $spec1 1 [/ [size $spec1] 2] 2]
    set amps1 [* [> $amps1 $threshold] $amps1]

    set buff2 [slice $sig2 $i $sz]
    set buff2 [* $bartlett $buff2]
    set spec2 [car2pol [fft $buff2]]
    set amps2 [slice $spec2 0 [/ [size $spec2] 2] 2]
    set phi2  [slice $spec2 1 [/ [size $spec2] 2] 2]

    set outamps [sqrt [* $amps1 $amps2]]
    
    set outphi $phi2
    set outbuff [ifft [pol2car [interleave $outamps $outphi]]]
    set outbuff [* $bartlett $outbuff]
    assign $outsig [+ [slice $outsig $i [size $outbuff]] $outbuff] $i [size $outbuff]
    set i [+ $i $hop]]
}

sndwrite 44100 "xsynth.wav" $outsig
