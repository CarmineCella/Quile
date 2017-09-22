# assignment and print
set a 12
puts $a 

#string interpolation
set a pu
set b ts
$a$b "hallo world!"

#command substitution
proc make_sum {a b} {
	set c [expr $a + $b]
	return $c
}
puts [make_sum 2 3]

#substitution in strings
puts "this sum is [make_sum 5 [make_sum 5 3]] and vars is $a"
puts "string in \"string\""

#nesting and substitution
puts [make_sum [make_sum 4 5] 6]

#flow control
if {[make_sum 3 4] > 5} {
	puts "grater"
} else {
	puts "lesser"
}

#loops
set x 0
while {$x < 10} {
    puts "x is $x"
    incr x
}

#lists
set colorList1 {red green blue}
set colorList2 [list red green blue]
set colorList3 [split "red_green_blue" _]
puts $colorList1
puts $colorList2
puts $colorList3
lappend colorList3 rainbow
puts $colorList3
puts [lindex $colorList3 2]

puts {this is              some space}