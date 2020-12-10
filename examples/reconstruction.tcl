source "stdlib.tcl"

set sr 44100
set samps [* 1.2 $sr]

set tab1 [gen 4096 1]

puts "analysing......"
set input [car [cdr [sndread "../sounds/gong_c#3.wav"]]]
set spec [fft $input]
set polar [car2pol $spec]]
set mag [slice $polar 0 [/ [size $polar] 2] 2]

set mag [slice $mag 128 2048]

set fftfreqs [bpf 0 [/ [size $spec] 2] 22050]
set fftfreqs [slice $fftfreqs 128 [size $mag]]

set amps [list]
set i 0
while {< $i [size $mag]} {
    set v [slice $mag $i 1]
    ljoin $amps [bpf $v $samps $v]
    set i [+ $i 1]
}
set freqs [list]
set i 0
while {< $i [size $mag]} {
    set v [slice $fftfreqs $i 1]
    ljoin $freqs [bpf $v $samps $v]
    set i [+ $i 1]
}

puts "done" $nl "synthesising..."
set out [oscbank $sr $amps $freqs $tab1]
set norm 0.8

set fade [bpf $norm $samps 0.0]
set out [* $fade $out]
puts "done" $nl

sndwrite $sr "reconstruction.wav" $out

