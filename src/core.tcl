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
proc last {l} {lindex  $l [- [llength $l] 1]}
proc drop {l n} {
	if {<= $n 0} {
		fuse $l
	} else {
		drop [cdr $l] [- $n 1] 
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
proc unpack  {f l} {
	puts $f $nl $l $& $nl
	eval [fuse [list] $f $l]
}
proc pack {f} {$f $&}
