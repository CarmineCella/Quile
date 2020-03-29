// skicl.h
//
#ifndef SKICL_H
#define SKICL_H

#include <deque>
#include <string>
#include <sstream>
#include <stdexcept>
#include <fstream>
#include <memory>
#include <iostream>
#include <map>
#include <variant>

// TODO: primitive, if, while, upval, eval, serializzazione, test
// speed, tail ricorsione, return, break, continue, static scope, upvar?

// ast
struct Atom;
typedef std::shared_ptr<Atom> AtomPtr;
typedef double Real;
typedef AtomPtr (*Functor) (AtomPtr, AtomPtr);
enum AtomType {ELEMENT, COLL, LIST, FUNCTOR};
struct Atom {
private:
	// create only via factory methods
	struct _constructor_tag { explicit _constructor_tag() = default; }; 
public:	
	AtomType type;
	std::string value;
	std::deque<AtomPtr> block;
	Functor func;
	int minargs;
	static AtomPtr make_sequence (bool is_list = false) { 
		AtomPtr l = std::make_shared<Atom> (_constructor_tag{}); 
		l->type = is_list ? AtomType::LIST : AtomType::COLL;
		return l; 
	}
	static AtomPtr make_element (const std::string& v) { 
		AtomPtr s = std::make_shared<Atom> (_constructor_tag{});
		s->type = AtomType::ELEMENT; 
		s->value = v; 
		return s;
	}
	static AtomPtr make_functor (Functor a, int min = 0) {
		AtomPtr s = std::make_shared<Atom> (_constructor_tag{});
		s->type = AtomType::FUNCTOR;
		s->func = a; 
		s->minargs = min;
		return s;	
	}
};

// lexing
std::string get_token (std::istream& input) {
	std::stringstream accum;
	while (!input.eof ()) {
		char c = input.get ();
		switch (c) {
			// case '{': case '[': {
			// 	std::string valid_terminator = "}";	std::string invalid_terminator = "]";
			// 	if (c == '[') {
			// 		valid_terminator = "]";	invalid_terminator = "}";
			// 	}
		 //        accum << c;
	  //           while (!input.eof ()) {
	  //               std::string s = get_token(input);
	  //               accum << s << " ";
			// 		if (s == valid_terminator) break;	
			// 		if (s == invalid_terminator) {
			// 			std::stringstream err;
			// 			err << "illegal terminator " << invalid_terminator 
			// 				<< " in " << accum.str ();
			// 			throw std::runtime_error (err.str ());
			// 		}
	  //           }
			// 	return accum.str ();
			// }
			// break;  			
			case ']': case '\n': case '}': case '[': case '{':
 				if (accum.str ().size ()) {
					input.unget();
					return accum.str ();
				} else {
					accum << c;
					return accum.str ();
				}
			break;
		    case '#':
				while (c != '\n' && !input.eof ())  { c = input.get (); }
                input.unget();
			break;            
			case ' ': case '\t': case '\r':
				if (accum.str ().size ()) return accum.str ();
				else continue;
			break;   
			case '\"':
		        accum << c;
	            do  {
	                c = input.get ();
	                accum << c;
	            } while (c != '\"' && !input.eof ());
				return accum.str (); 
			break;             
			default:
				if (c > 0) accum << c;
		}
	}
	return accum.str ();
}

// parsing and evaluation
// AtomPtr check_args (const std::string& cmd, AtomPtr b, int i) {
//     if (b.size () != i) {
//         std::stringstream err;
//         err << "wrong number of arguments in " << cmd;
//         throw std::runtime_error (err.str ());
//     }
//     return b;
// }
AtomPtr read (std::istream& input) {
	std::string token = get_token (input);
	while (!input.eof ()) {
		if (token == "[" || token == "{") {
			std::string valid_terminator = "]";	std::string invalid_terminator = "}";
			if (token == "{") {
				valid_terminator = "}";	invalid_terminator = "]";
			}
			AtomPtr list = Atom::make_sequence(token == "[");
			AtomPtr ct = Atom::make_sequence ();
			while (!input.eof ()) {
				ct = read(input);
				if (ct->value == valid_terminator) break;	
				if (ct->value == invalid_terminator) {
					std::stringstream err;
					err << "illegal terminator " << invalid_terminator;
					throw std::runtime_error (err.str ());
				}
				list->block.push_back (ct);
			}
			if (ct->value != valid_terminator) {
				throw std::runtime_error ("unexpected EOF while parsing");
			}
			return list;
		} else {
			AtomPtr s = Atom::make_element (token);
			return s;
		}
	}
	return Atom::make_sequence ();
}
std::string eval (AtomPtr node, AtomPtr env) {
    
    while (!in.eof ()) {
 		std::string token = get_token(in);
 		if (token.size () == 0) continue;
 		if (token == "\n") break;
       	if (token[0] == '$') {
            std::string key = token.substr (1, token.size () - 1);
            if (nspace.variables.find (key) != nspace.variables.end ()) {
                b.push_back (nspace.variables[key]);
            } else {
                std::stringstream err;
                err << "undeclared identifier " << key;
                throw std::runtime_error (err.str ());
            }
        } else if (token[0] == '[') {
        	int p = token.find_first_of("[");
        	int e = token.find_last_of("]");
        	std::stringstream code;
        	code << token.substr(p + 1, e - 1);
        	b.push_back(eval (code, nspace));
        } else if (token[0] == '{') {
     		int p = token.find_first_of("{");
        	int e = token.find_last_of("}");
        	b.push_back(token.substr (p + 1, e - 1));        	
        } else if (token[0] == '\"') {
        	b.push_back(token.substr (1, token.substr ().size () - 2));
        } else b.push_back(token);
    }
    if (b.size () == 0) return "";
    std::string cmd = b.at (0);
    b.pop_front ();
    if (nspace.proceduers.find (cmd) != nspace.proceduers.end ()) {
        std::stringstream argstream (nspace.proceduers[cmd].at (0));
        std::stringstream code (nspace.proceduers[cmd].at (1));
        Block args;
        while (!argstream.eof ()) {
        	std::string a = get_token (argstream);
            if (a.size ()) args.push_back (a);
        }
        Namespace nnspace = nspace;
        if (b.size () != args.size ()) {
            std::stringstream err;
            err << "invalid number of arguments in " << cmd;
            throw std::runtime_error (err.str ());
        }
        for (unsigned i = 0; i < b.size (); ++i) {
            nnspace.variables[args.at (i)] = b.at (i);
        }
        std::string res;
        while (!code.eof ()) {
        	if (code.str () == "\n") break;
            res = eval (code, nnspace);
        }
        return res;
    } else if (nspace.functors.find (cmd) != nspace.functors.end ()) {
        return nspace.functors[cmd](b, nspace);
    } else {
        std::stringstream err;
        err << "undeclared command " << cmd;
        throw std::runtime_error (err.str());
    }
    return "";
}

// functors
std::string fn_puts (AtomPtr b, Namespace& nspace) {
    for (unsigned i = 0; i < b.size (); ++i) std::cout << b.at (i);
    return "";
}
// std::string fn_set (AtomPtr b,  Namespace& nspace) {
//     check_args ("set", b, 2);
//     std::string key = b.at (0);
//     nspace.variables[key] = b.at (1);
//     return b.at (1);
// }
// std::string fn_proc (AtomPtr b, Namespace& nspace) {
//     check_args ("proc", b, 3);
//     std::string key = b.at (0);
//     b.pop_front ();
//     nspace.proceduers[key] = b;
//     return key;
// }
// Real to_number (const std::string& s) {
// 	std::istringstream iss (s.c_str ());
// 	Real dummy;
// 	iss >> std::noskipws >> dummy;
// 	if (!(iss && iss.eof ())) {
//         std::stringstream err;
//         err << "invalid argument " << s;
//         throw std::runtime_error (err.str ());
//     }
//     return atof (s.c_str ());
// }
// template <char op>
// std::string fn_binop (AtomPtr b,  Namespace& nspace) {
//     check_args ("bin_op", b, 2);
//     std::stringstream val;
//     Real first = to_number (b.at (0)); 
//     Real second = to_number (b.at (1));
//     switch (op) {
//         case '+':  val << first + second;  break;
//         case '-':  val << first - second;  break;
//         case '*':  val << first * second;  break;
//         case '/':  val << first / second;  break;
//         case '<':  val << (first < second);  break;
//         case '>':  val << (first > second);  break;
//         case 'L':  val << (first <= second);  break;
//         case 'G':  val << (first >= second);  break;
//         case '=':  val << (first == second);  break;
//     }
//     return val.str ();
// }
// std::string load (const std::string& name, Namespace& nspace) {
// 	std::ifstream in (name.c_str ());
// 	if (!in.good ()) {
// 		std::string longname = getenv("HOME");
// 		longname += "/.skicl/" + name;
// 		in.open (longname.c_str());
// 		if (!in.good ()) return "";
// 	}
//     std::string res;
// 	while (!in.eof ()) {
// 		res = eval (in, nspace);
// 	}
// 	return res;
// }
// std::string fn_load (AtomPtr b,  Namespace& nspace) {
//     check_args ("load", b, 1);
//     return load (b.at (0), nspace);
// }

// interface
void init_namespace (Namespace& nspace) {
    nspace.functors["puts"] = fn_puts;
    // nspace.functors["load"] = fn_load;
    // nspace.functors["set"] = fn_set;
    // nspace.functors["proc"] = fn_proc;
    // nspace.functors["+"] = fn_binop<'+'>;
    // nspace.functors["-"] = fn_binop<'-'>;
    // nspace.functors["*"] = fn_binop<'*'>;
    // nspace.functors["/"] = fn_binop<'/'>;
    // nspace.functors["<"] = fn_binop<'<'>;
    // nspace.functors[">"] = fn_binop<'>'>;
    // nspace.functors["<="] = fn_binop<'L'>;
    // nspace.functors[">="] = fn_binop<'G'>;
    // nspace.functors["=="] = fn_binop<'='>;

    // nspace.variables["nl"] = "\n";
}
void repl (std::istream& in, Namespace& nspace) {
    while (true) { 
        std::cout << ">> ";
        try {
            std::cout << eval (in, nspace) << std::endl;
        } catch (std::exception& err) {
            std::cout << "error: " << err.what () << std::endl;
        }
    }  
}


#endif // SKICL_H