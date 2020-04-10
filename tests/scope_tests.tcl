# {Tests for the Quile scope policies}
#
# (c) 2020, www.quile.org
#
source "core.tcl"

set b 5

proc static_foo {} {
	pass [+ $b 5]
}

proc static_bar {} {
	set b 2
	pass [static_foo]
}

dynamic dynamic_foo {} {
	pass [+ $b 5]
}

proc dynamic_bar {} {
	set b 2
	pass [dynamic_foo]
}
 
puts $nl "--- static vs dynamic scope ---" $nl

test {static_foo}{10}
test {static_bar}{10}

test {dynamic_foo}{10}
test {dynamic_bar}{7}


puts $nl "---  local scope ---" $nl
test {eval {+ 1 2}}{3}
test {
	let {
		set hhh 123
		pass [+ $hhh $hhh]
	}
} {246}
test {unset hhh}{0}

puts $nl "ALL TESTS PASSED" $nl $nl

# eof

