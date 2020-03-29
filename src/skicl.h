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
// speed, block ricorsione, return, break, continue, static scope, upvar?

// ast
struct Atom;
typedef std::shared_ptr<Atom> AtomPtr;
typedef double Real;
typedef AtomPtr (*Functor) (AtomPtr, AtomPtr);
enum AtomType {ELEMENT, COLL, LIST, LAMBDA, FUNCTOR};
struct Atom {
private:
	// create only via factory methods
	struct _constructor_tag { explicit _constructor_tag() = default; }; 
public:	
	Atom (_constructor_tag) {
		 token = ""; func = nullptr; 
	}	
	AtomType type;
	std::string token;
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
		s->token = v; 
		return s;
	}
	static AtomPtr make_lambda (AtomPtr args, AtomPtr body, AtomPtr closure) {
		AtomPtr s = std::make_shared<Atom> (_constructor_tag{}); 
		s->type = AtomType::LAMBDA;
		s->block.push_back(args);
		s->block.push_back(body);
		s->block.push_back(closure);
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
bool is_null (AtomPtr node) { 
	return !node || 
		((node->type == AtomType::COLL || node->type == AtomType::LIST) 
			&& node->block.size () == 0);
}
int atom_eq (AtomPtr x, AtomPtr y) {
	if (x->type != y->type) return 0;
	switch (x->type) {
	    case AtomType::ELEMENT: return (x->token == y->token);
	    case AtomType::COLL: case AtomType::LIST:  {
			if (x->block.size () != y->block.size ()) { return 0; }
			for (unsigned i = 0; i < x->block.size (); ++i) {
				if (!atom_eq (x->block.at(i), y->block.at(i))) { return 0; }
			}
			return 1;
		}
		case AtomType::LAMBDA: 
			if (x->block.size () < 2 || y->block.size () < 2) return 0;
			return (atom_eq (x->block.at(0), y->block.at(0)) 
				&& atom_eq (x->block.at(1), y->block.at(1)));
	    case AtomType::FUNCTOR: return (x->func == y->func);
		default:
		return 0;
	}
}
// lexing
std::string get_token (std::istream& input) {
	std::stringstream accum;
	while (!input.eof ()) {
		char c = input.get ();
		switch (c) { 			
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
std::ostream& print (AtomPtr node, std::ostream& out);
void error (const std::string& err, AtomPtr node) {
	std::stringstream tmp;
	tmp << err;
	if (node) {
		std::stringstream tmp2;
		print (node, tmp2);
		tmp << " " << tmp2.str ();
	}
	throw std::runtime_error (tmp.str ());
}
AtomPtr read (std::istream& input, const std::string& terminator = "\n") {
	AtomPtr code = Atom::make_sequence (true); // executable
	while (!input.eof ()) {
		std::string token = get_token (input);
		if (token.size() == 0) continue;
		if (token == "\n") break;
		if (token == "[" || token == "{") {
			std::string valid_terminator = "]";	std::string invalid_terminator = "}";
			if (token == "{") valid_terminator = "}"; invalid_terminator = ")";
			AtomPtr list = Atom::make_sequence(token == "[");
			while (!input.eof ()) {
				AtomPtr ct = read(input, valid_terminator);
				list->block.push_back (ct);
				if (ct->block.size () > 0 && 
					ct->block.at(ct->block.size () - 1)->token == invalid_terminator) {
					error ("illegal terminator used in ", list);
				}
				print  (ct, std::cout) << std::endl;
				std::cout << valid_terminator << "----" << std::endl;
				if (ct->block.size () > 0 && 
					ct->block.at (ct->block.size () - 1)->token == valid_terminator) {
					ct->block.pop_back();
					break;
				} 
			}

			code->block.push_back(list);
			return code;
		} else {
			AtomPtr s = Atom::make_element (token);
			code->block.push_back(s);
		}
	}
	return code;
}
AtomPtr assoc (AtomPtr sym, AtomPtr env) {
	AtomPtr r = Atom::make_sequence ();;
	for (unsigned i = 1; i < env->block.size () / 2 + 1; ++i) {
		if (atom_eq (env->block.at (i * 2 - 1), sym)) return env->block.at (i * 2);
	}
	if (!is_null (env->block.at (0))) return assoc (sym, env->block.at (0));
	error ("unbound identifier", sym);
	return Atom::make_sequence ();; // not reached
}
AtomPtr extend (AtomPtr key, AtomPtr val, AtomPtr env, bool recurse = false) {
	for (unsigned i = 1; i <= env->block.size () / 2; ++i) {
		if (atom_eq (env->block.at (2 * i - 1), key)) {
			env->block.at (2 * i) = val; 
			return val;
		}
	}
	if (recurse) {
		if (!is_null(env->block.at(0))) return extend(key, val, env->block.at(0), recurse);
	} 
	env->block.push_back (key); env->block.push_back (val);
	return val;
}
AtomPtr eval (AtomPtr node, AtomPtr env) {
    if (is_null (node)) return Atom::make_sequence();
    AtomPtr evaluated = Atom::make_sequence();
    for (unsigned i = 0; i < node->block.size (); ++i) {
    	AtomPtr c = node->block.at (i);
    	if (c->type == LIST) {
    		for (unsigned j = 0; i < c->block.size (); ++i) {
    			print (c->block.at (j), std::cout) << std::endl;
    			evaluated->block.push_back(eval (c->block.at (j), env));
    		}
    	} else if (c->type == COLL) {
    		for (unsigned j = 0; i < c->block.size (); ++i) {
    			evaluated->block.push_back(c->block.at (j));
    		}
    	} else {
    		if (c->token[0] == '$') {
    			c->token = c->token.substr(1, c->token.size () -1);
    			evaluated->block.push_back (assoc (c, env));
    		} //else if (c->token == "\n") continue;
    		else {
    			evaluated->block.push_back(c);
    		}
    	}
    }
    if (evaluated->block.size () == 0) return Atom::make_sequence();
    
    AtomPtr cmd = assoc (evaluated->block.at (0), env);
    evaluated->block.pop_front ();
    if (cmd->type == LAMBDA) {
    } else if (cmd->type == FUNCTOR) {
    	return cmd->func (evaluated, env);
    } else error ("function expected in", cmd);
    return Atom::make_sequence();
}

// functors
std::ostream& print (AtomPtr node, std::ostream& out) {
	switch (node->type) {
		case ELEMENT:
			out << ":"<< node->token <<":";
		break;
		case LIST: case COLL:
			out << (node->type == LIST ? "[" : "{");
			for (unsigned i = 0; i < node->block.size (); ++i) {
				print (node->block.at (i), out) << " ";
			}
			out << (node->type == LIST ? "]" : "}");
		break;
		case LAMBDA:
			if (node->block.size () < 2) return out;
			print (node->block.at (0), out) << std::endl;
			print (node->block.at (1), out) << std::endl;
		break;
		case FUNCTOR: 
			out << "<" << (std::hex) << &node->func << ", " 
				<< node->minargs << " min args>";
		break;
	}
	return out;
}
AtomPtr fn_puts (AtomPtr b, AtomPtr env) {
    for (unsigned i = 0; i < b->block.size (); ++i) {
    	print (b->block.at (i), std::cout);
    }
    return Atom::make_element("");
}
AtomPtr fn_set (AtomPtr b,  AtomPtr env) {
    // check_args ("set", b, 2);
    return extend (b->block.at (0), b->block.at (1), env);
}
template <bool dynamic>
AtomPtr fn_lambda (AtomPtr n, AtomPtr env) {
	return Atom::make_lambda (n->block.at (0), n->block.at (1), 
		dynamic ? Atom::make_sequence () : env);	
}
// std::string fn_proc (AtomPtr b, Namespace& nspace) {
//     check_args ("proc", b, 3);
//     std::string key = b.at (0);
//     b.pop_front ();
//     nspace.proceduers[key] = b;
//     return key;
// }
Real to_number (const std::string& s) {
	std::istringstream iss (s.c_str ());
	Real dummy;
	iss >> std::noskipws >> dummy;
	if (!(iss && iss.eof ())) {
        std::stringstream err;
        err << "invalid argument " << s;
        throw std::runtime_error (err.str ());
    }
    return atof (s.c_str ());
}
template <char op>
AtomPtr fn_binop (AtomPtr b,  AtomPtr env) {
    // check_args ("bin_op", b, 2);
    std::stringstream val;
    Real first = to_number (b->block.at (0)->token); 
    Real second = to_number (b->block.at (1)->token);
    switch (op) {
        case '+':  val << first + second;  break;
        case '-':  val << first - second;  break;
        case '*':  val << first * second;  break;
        case '/':  val << first / second;  break;
        case '<':  val << (first < second);  break;
        case '>':  val << (first > second);  break;
        case 'L':  val << (first <= second);  break;
        case 'G':  val << (first >= second);  break;
        case '=':  val << (first == second);  break;
    }
    return Atom::make_element(val.str ());
}
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
void add_func (const std::string& name, Functor f, int minargs, AtomPtr env) {
	AtomPtr op = Atom::make_functor (f, minargs); 
	op->token = name;
	extend (Atom::make_element (name), op, env);
}

AtomPtr make_env () {
	AtomPtr env = Atom::make_sequence ();
	env->block.push_back (Atom::make_sequence ()); // parent

    add_func("puts", fn_puts, 0, env);
    add_func("set", fn_set, 2, env);
    add_func("\\", fn_lambda<false>, 2, env);
    // nspace.functors["load"] = fn_load;
    // nspace.functors["proc"] = fn_proc;
    add_func("+", fn_binop<'+'>, 2, env);
    // nspace.functors["-"] = fn_binop<'-'>;
    // nspace.functors["*"] = fn_binop<'*'>;
    // nspace.functors["/"] = fn_binop<'/'>;
    // nspace.functors["<"] = fn_binop<'<'>;
    // nspace.functors[">"] = fn_binop<'>'>;
    // nspace.functors["<="] = fn_binop<'L'>;
    // nspace.functors[">="] = fn_binop<'G'>;
    // nspace.functors["=="] = fn_binop<'='>;

    // nspace.variables["nl"] = "\n";
    return env;
}
void repl (std::istream& in, AtomPtr env) {
    while (true) { 
        std::cout << ">> ";
        try {
            print (eval (read (in), env), std::cout) << std::endl;
            //print (read (in), std::cout) << std::endl;
        } catch (std::exception& err) {
            std::cout << "error: " << err.what () << std::endl;
        }
    }  
}


#endif // SKICL_H	