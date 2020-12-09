# {Quile standard library}
#
# (c) 2020, www.quile.org
#

# atoms
set true 1
set false 0

# macros
set proc [@ {name args body} {
	updef $name [\ $args $body]
}]
set dynamic [@ {name args body} {
	updef $name [@ $args $body]
}]
dynamic let {b} { 
	[\ {} {eval $b}]
}

# lists
proc lindex {l i} {car [lrange $l $i 1]}
proc cdr {l} {lrange $l 1 [- [llength $l] 1]}
proc second {l} {car [cdr $l]}
proc llast {l} {lindex  $l [- [llength $l] 1]}
proc ltake {l n} {lrange $l 0 $n}
proc ldrop {l n} {
	if {<= $n 0} {
		ljoin $l
	} else {
		ldrop [cdr $l] [- $n 1] 
	}
}
proc lrepeat {n l} {
	if {> $n 0}	{
		ljoin [list] $l [lrepeat [- $n 1] $l]
	}
}
proc lsplit {l n} {
	list [lrange $l 0 $n] [ldrop $l $n]
}
proc lmatch {e l} {
	if {eq $l {}} {
		list
	} else {
		if {eq $e [car $l]} {
			ljoin $l
		} else {
			lmatch $e [cdr $l]
		}
	}
}
proc lelem {x l} {
	if {eq 0 [llength [lmatch $x $l]]} {
		array $false
	} else {
		array $true
	}
}
proc lreverse {l} {
	if {eq {} $l} {
		list
	} else {
		ljoin [lreverse [cdr $l]] [car $l]
	}
}

# higher order
proc map {f l} {
	if {eq {} $l} {
		list
	} else {
		ljoin [list [$f [car $l]]] [map $f [cdr $l]]
	}
}
proc filter {f l} {
	if {eq {} $l} {
    	list
   	} else {
    	ljoin [if {$f [car $l]} {list [lindex $l 0]} else {list}] [filter $f [cdr $l]]
    }
}
proc unpack  {f l} {eval [ljoin [list] $f $l]}
proc pack {f} {$f $&}
proc foldl {f z l} {
	if {eq {} $l} {
		list $z
	} else {
		foldl $f [$f $z [car $l]] [cdr $l]
	}
}
proc flip {f a b} {$f $b $a}
proc comp {f g x} {$f [$g $x]}

# logical operators
proc not {x} {if {array $x} {array 0} else {array 1}}
proc or {x y}  {if {+ $x $y} {array 1} else {array 0}}
proc and {x y} {if {* $x $y} {array 1} else {array 0}}
proc != {n1 n2} {not [eq $n1 $n2]}

# miscellaneous
proc lsum {l} {foldl + 0 $l}
proc lprod {l} {foldl * 1 $l}
proc case {x} {
 	if {eq $& {}} {
 		throw "case not found"
 	} else {
 		if {eq $x [car [car $&]]} {
 			second [car $&]
 		} else {
 			unpack case [ljoin [list] $x [cdr $&]]
 		}
    }
}
proc test {x y} {
	set res [ljoin [list] [eval $x]]
	if {eq $res $y} {
		puts $x " passed" $nl 
	} else {
		throw [tostr "*** FAILED *** " $x ": "  $res " vs " $y]
	}
}	

# arrays
proc interleave {in1 in2} {
    set out [bpf 0 [+ [size $in1] [size $in2]] 0]
    assign $out $in1 0 [size $in1] 2
    assign $out $in2 1 [size $in2] 2
    array $out
}

# eof

