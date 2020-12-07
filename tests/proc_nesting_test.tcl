source "core.tcl"

set b 10

puts $b $nl

proc level1 {x} {
    proc level2 {x} {
        proc level3 {x}{
            puts "level 3 " $x $nl
        }
        puts "level 2 " $x $nl
        level3 30
    }
    puts "level 1 " $x $nl
    level2 20
}

level1 $b

