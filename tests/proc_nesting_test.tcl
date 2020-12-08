source "core.tcl"

proc level1 {x} {
    proc level2 {x} {
        proc level3 {x}{
            puts "level 3 " $x $nl
            + $x 3
        }
        puts "level 2 " $x " "
        level3 [+ $x 2]
    }
    puts "level 1 " $x " "
    level2 [+ $x 1]
}

test {level1 10}{16}

puts $nl "ALL TESTS PASSED" $nl $nl
