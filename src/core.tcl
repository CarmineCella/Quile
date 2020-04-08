# --------------------
# [skicl] core library
# --------------------
#
# (c) 2020, www.skicl.org, www.carminecella.com
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
proc car {x} {lindex $x 0}
proc cdr {x} {lrange $x 1 [- [llength $x] 1]}
proc second {x} {car [cdr $x]}
proc last {l} {lindex  $l [- [llength $l] 1]}
proc drop {l n} {
	if {<= $n 0} {
		fuse $l
	} else {
		drop [cdr $l] [- $n 1] 
	}
}
proc rep {n l} {
	if {> $n 0}	{
		fuse [list] $l [dup [- $n 1] $l]
	}
}

proc split {l n} {
	list [lrange $l 0 [- $n 1]] [drop $l $n]
}
proc match {e l} {
	if {eq $l {}} {
		list
	} else {
		if {eq $e [car $l]} {
			fuse $l
		} else {
			match $e [cdr $l]
		}
	}
}
proc elem {x l} {
	if {eq 0 [llength [match $x $l]]} {
		fuse $false
	} else {
		fuse $true
	}
}
proc reverse {l} {
	if {eq {} $l} {
		list
	} else {
		fuse [reverse [cdr $l]] [car $l]
	}
}

# higher order
proc map {f l} {
	if {eq {} $l} {
		list
	} else {
		fuse [list [$f [car $l]]] [map $f [cdr $l]]
	}
}
proc filter {f l} {
	if {eq {} $l} {
    	list
   	} else {
    	fuse [if {$f [car $l]} {list [lindex $l 0]} else {list}] [filter $f [cdr $l]]
    }
}
proc unpack  {f l} {eval [fuse [list] $f $l]}
proc pack {f} {$f $&}
proc foldl {f z l} {
	if {eq {} $l} {
		fuse $z
	} else {
		foldl $f [$f $z [car $l]] [cdr $l]
	}
}
proc flip {f a b} {$f $b $a}
proc comp {f g x} {$f [$g $x]}

# logical operators
proc not {x} {if {fuse $x} {fuse 0} else {fuse 1}}
proc or {x y}  {if {+ $x $y} {fuse 1} else {fuse 0}}
proc and {x y} {if {* $x $y} {fuse 1} else {fuse 0}}
proc != {n1 n2} {not [eq $n1 $n2]}

# miscellaneous
set mul-neg [comp - [unpack *]]
proc sum {l} {foldl + 0 $l}
proc prod {l} {foldl * 1 $l}
# proc case {x} {
# 	puts [car $&] " **** " $nl
#  	if {eq $& {}} {
#  		throw [tostr "case not found " $x]
#  	} else {
#  		puts [car[car $&]] " --- " $nl
#  		if {eq $x [car[car $&]]} {
#  			second [car $&]
#  		} else {
#  			puts [lappend [list] $x [cdr $&]]
#  			unpack case [lappendx $x [cdr $&]]
#  		}
#     }
# }
proc test {x y} {
	if {eq (fuse [list] [eval $x]] $y} {
		puts $x " passed" $nl 
	} else {
		throw [tostr "*** FAILED *** " $x]
	}
}	

# eof

