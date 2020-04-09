# puts "set proc" $nl
set proc [@ {name args body} {
	updef $name [\ $args $body]
}]

# puts "set dynamic" $nl
set dynamic [@ {name args body} {
	updef $name [@ $args $body]
}]

dynamic let {b} { 
	[\ {} {eval $b}]
}


let {
	set kick 100
	puts "local value " $kick $nl
}

# puts $kick $nl # not defined outside of let

proc car {x} {lindex $x 0}
proc cdr {x} {lrange $x 1 [- [llength $x] 1]}
proc map {f l} {
	if {eq {} $l} {
		concat {}
	} else {
		concat [list [$f [car $l]]] [map $f [cdr $l]]
	}
}

puts "map " [map [\{x}{* $x $x}] {1 2 3 4 5}] $nl

proc filter {f l} {
	if {eq {} $l} {
    	concat {}
   	} else {
    	concat [if {$f [car $l]} {list [lindex $l 0]} else {list}] [filter $f [cdr $l]]
    }
}

puts "filter " [filter [\ {x} {> $x 5}] {5 2 11 -7 8 1}] $nl

proc unpack  {f l} {eval [concat {} $f $l]}
proc pack {f} {$f $&}

puts "unpack " [unpack + [list 4 5]] $nl

puts "set foo" $nl

set b 5
dynamic foo {} {
   set a [+ $b 5]
}
 
puts "set bar" $nl 
proc bar {} {
   set b 2
   foo
}

puts $foo $bar $nl $nl
puts "calls" $nl
puts [foo] $nl
# this gives 7 instead of 10
puts [bar] $nl $nl

set b 10
puts "start from " $b $nl
if {> $b 5} {
	set b [+ $b 10]
	puts "go higher" $nl
}

while {> $b 0} {
	set b [- $b 1]
	if {eq $b 5} {
		# puts "stop" $nl
		break
	} 
	puts $b $nl
}

proc count-down-from {n} {
	\ {} {
		setrec n [- $n 1]
	}
} 

set count-down-from-4 [count-down-from 4]

puts "count down" $nl
puts [count-down-from-4] $nl
puts [count-down-from-4] $nl
puts [count-down-from-4] $nl


proc fib {x} {
    if {<= $x 1} {
        concat 1
    } else {
        + [fib [- $x 1]] [fib [- $x 2]]
    }
}

puts "fibonacci " [fib 20] $nl

proc alpha {x} {
	proc beta {x} {
		puts $x $nl
	}
	
	puts "call beta "
	beta 4
}

alpha 5

#error
# beta 4 

puts "end" $nl
