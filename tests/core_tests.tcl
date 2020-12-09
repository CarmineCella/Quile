# {Tests for the Quile core library}
#
# (c) 2020, www.quile.org
#

source "core.tcl"

puts $nl "--- builtin operators ---" $nl

test {list 1 2 3 4}{1 2 3 4} 
test {ljoin [list] 1 2 {3 4}}{1 2 3 4}
test {car {1 b c}}{ 1}
test {car {{}}} {}
test {cdr {}}{}
test {cdr {a b c}}{b c}
test {cdr {}} {}
test {if {eq 1 1} {array 1}}{1}
test {if {eq 1 0} {array 1}}{}
test {if {eq 1 0} {array 1} else {array 2}}{2}
set dummy 21
test {setrec dummy 22}{22}
test {eq 1 1}{1}
test {eq 1 2}{0}
test {eq 1 $dummy}{0}
test {eq $dummy $dummy}{1}
test {eq {1 2 3}{a b c}}{0}
test {eq {a b c}{a b c}}{1}
test {eq {a b c}{a b}}{0}
test {eq + +}{1}
test {eq $+ $+}{1}
test {eq + -}{0}
test {eq $+ $-}{0}
test {eq "alpha" "alpha"}{1}
test {eq "alpha" "beta"}{0}
test {catch {except1 } {throw {except1}} {list 1}}{1}
test {catch "except2" {throw "except2"} {list 1}}{1}
test {info exists dummy}{1}
test {info exists not_defined}{0}
test {info vars "wrong_pattern" }{}
test {unset dummy} {1}
test {unset dummy} {0}
test {info exists dummy}{0}
test {info typeof {a b c}} {list}
test {info typeof {1 2}} {list}
test {info typeof {}} {list}
test {info typeof 3} {array}
test {info typeof $*} {builtin}
test {info typeof [\{x}{+ $x $x}]} {proc}
test {info typeof "hallo"} {string}
test {info typeof a} {symbol}
test  {+ 1 [- 3 [* 5 [/ 6 3]]]} {-6} 
# test {sum [- [array 1 2 3] [array 4 4 4]]} {-6}
test {-  0 3 }{-3}
test {sum [< [array 1 2 3 4][array 5 5 5 5]]}{4}
test {sum [<= [array 1 2 3 4][array 5 5 5 5]]}{4}
test {sum [> [array 1 2 3 4][array 1 1 1 1]]}{3}
test {sum [>= [array 1 2 3 4][array 1 1 1 1]]}{4}
test {min [array -4 0 4]}{-4}
test {max [array -4 0 4]}{4}
test {mean [array -4 0 4]}{0}
test {abs [array -4]}{4}
test {sqrt [array 16]}{4}
test {sum [* [array 1 2 3 4][array 1 2 3 4]]}{30}
test {tostr "hallo " "world!"}{"hallo world!"}
test {string range "hallo world!" 2 5}{"llo w"}
test {string find "hallo world!""lo" }{3}
test {string regex "1231" "^(\d)\d"}{"12" "1"}
test {string regex "I am looking for GeeksForGeeks articles" "Geek[a-zA-Z]+"}{"GeeksForGeeks"}
test {string regex "GeeksForGeeks" "(Geek)(.*)" } {"GeeksForGeeks" "Geek" "sForGeeks"}

puts $nl "---  atoms ---" $nl
test {list $true}{1}
test {list $false}{0}

puts $nl  "---  lists ---" $nl
test {llength {a b c d}}{4}
test {llength {}}{0}
test {llength {0}}{1}

test {lindex {a b c d} 2}{c}
test {lindex {a b c d} -1}{}
test {lindex {a b c d} 20}{}

test {llast {1 2 3 4}}{4}
test {llast {a b c d}}{d}

test {ltake {1 a 2 b 3 c} 2}{1 a}
test {ltake {1 a 2 b 3 c} -1}{}
test {ltake {1 a 2 b 3 c} 10}{}

test {ldrop {1 a 2 b 3 c} 2}{2 b 3 c}
test {ldrop {1 a 2 b 3 c} -1}{1 a 2 b 3 c}
test {ldrop {1 a 2 b 3 c} 10}{}

test {lsplit {a b 2 3} 2}{{a b} {2 3}}
test {lsplit {a b 2 3} 3}{{a b 2} {3}}
test {lsplit {a b 2 3} -3}{{} {a b 2 3}}
test {lsplit {a b 2 3} 6}{{}{}}}}

test {lmatch a {1 2 a b}}{a b}
test {lmatch z {1 2 a b}}{}
test {lmatch 1 {1 2 a b}}{1 2 a b}

test {lelem b {1 2 a b}}{1}
test {lelem h {1 2 a b}}{0}
test {lelem 2 {1 2 a b}}{1}

test {lreverse {a b 2 c}}{c 2 b a}
test {lreverse {}}{}

test {comp [\{x}{- 0 $x}] [\{x}{* $x $x}] 5}{-25}

puts $nl "---  higher order operators ---" $nl
test {map [\{x}{* $x $x}] {1 2 3 4}}{1 4 9 16}
test {map [\{x}{* $x $x}] {2 3 4 5}}{4 9 16 25}

test {filter [\{x}{<= $x 2}] {1 2 3 4}} {1 2}
test {filter [\{x}{> $x 2}] {1 2 3 4}} {3 4}
test {filter [\{x}{<= $x 2}] {12 22 3 4}} {}

test {unpack + {1 2}}{3}
test {unpack * {3 4}}{12}
test {pack car 1 2 3 4} {1}
test {pack cdr 1 2 3 4} {2 3 4}
test {pack llength 1 2 {a} 3}{4}

test {foldl + 0 {1 2 3 4}}{10}
test {foldl * 1 {1 2 3 4}}{24}

puts $nl "--- logical operators ---" $nl
test {not 0}{1}
test {not 0}{1}
test {or 0 1}{1}
test {or 1 0}{1}
test {or 0 0}{0}
test {or 1 1 }{1}
test {and 0 1}{0}
test {and 1 0}{0}
test {and 0 0}{0}
test {and 1 1 }{1}
test {!= {1 2}{1 2}}{0}
test {!= {a b c}{a b c}}{0}
test {!= {a b c}{a b 1}}{1}

puts $nl "--- others ---" $nl
test {lsum {1 2 3}}{6}
test {lprod {1 2 3}}{6}
proc day-name {x} {
  case $x {1 "Monday"} {2 "Tuesday"} {3 "Wednesday"} {4 "Thursday"} {5 "Friday"} {6 "Saturday"} {7 "Sunday"}
}
test {day-name 1}{"Monday"}
test {day-name 4}{"Thursday"}
test {day-name 7}{"Sunday"}

puts $nl "ALL TESTS PASSED" $nl $nl

# eof



