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
// speed, return, break, continue, upvar?

// ast
struct Atom;
typedef std::shared_ptr<Atom> AtomPtr;
typedef double Real;
typedef AtomPtr (*Builtin) (AtomPtr, AtomPtr);
enum AtomType {ELEMENT, COLL, LIST, PROC, BUILTIN};
const char* TYPE_NAMES[] = {"element", "coll", "list", "proc",  "builtin"};

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
	Builtin func;
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
		s->type = AtomType::PROC;
		s->block.push_back(args);
		s->block.push_back(body);
		s->block.push_back(closure);
		return s;	
	}	
	static AtomPtr make_builtin (Builtin a, int min = 0) {
		AtomPtr s = std::make_shared<Atom> (_constructor_tag{});
		s->type = AtomType::BUILTIN;
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
		case AtomType::PROC: 
			if (x->block.size () < 2 || y->block.size () < 2) return 0;
			return (atom_eq (x->block.at(0), y->block.at(0)) 
				&& atom_eq (x->block.at(1), y->block.at(1)));
	    case AtomType::BUILTIN: return (x->func == y->func);
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
				return accum.str ().substr(1, accum.str ().size ()  - 2); 
			break;             
			default:
				if (c > 0) accum << c;
		}
	}
	return accum.str ();
}

// parsing and evaluation
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
AtomPtr args_check (AtomPtr node, unsigned ct, AtomPtr ctx = Atom::make_sequence ()) {
	if (node->block.size () < ct) {
		error ("wrong number of arguments in", is_null(ctx) ? node : ctx);
	}
	return node;
}
AtomPtr type_check (AtomPtr node, AtomType type) {
	if (is_null (node)) return node;
	if (node->type != type) {
		std::stringstream err;
		err << "invalid type ";
		err << TYPE_NAMES[static_cast<typename 	std::underlying_type<AtomType>::type>(node->type)];
		err << " [expected " << TYPE_NAMES[static_cast<typename 
		std::underlying_type<AtomType>::type>(type)] << "] in";
		error (err.str (), node);
	}
	return node;
}
AtomPtr read (std::istream& input, const std::string& terminator = "\n") {
	AtomPtr code = Atom::make_sequence (true); // executable
	while (!input.eof ()) {
		std::string token = get_token (input);
		if (token.size() == 0) continue;
		if (token == terminator) break;
		if (token == "[" || token == "{") {
			std::string valid_terminator = "]";
			if (token == "{") valid_terminator = "}"; 
			AtomPtr ct = read(input, valid_terminator);
			AtomPtr outer = Atom::make_sequence(token == "[");
			if (token == "{") {
	 			AtomPtr inner = Atom::make_sequence(true); // always executable
				for (unsigned t = 0; t < ct->block.size ();  ++t) {
					if (ct->block.at(t)->token == "\n") {
						if (!is_null (inner)) outer->block.push_back(inner);
						inner = Atom::make_sequence(true);
					} else inner->block.push_back(ct->block.at(t));
				}
				if (!is_null (inner)) outer->block.push_back(inner);
				code->block.push_back(outer);
			} else {
				code->block.push_back(ct);
			}	
		} else {
			code->block.push_back(Atom::make_element (token));
		}
	}
	return code;
}
AtomPtr assoc (const std::string& sym, AtomPtr env) {
	AtomPtr r = Atom::make_sequence ();;
	for (unsigned i = 1; i < env->block.size () / 2 + 1; ++i) {
		if (env->block.at (i * 2 - 1)->token == sym) return env->block.at (i * 2);
	}
	if (!is_null (env->block.at (0))) return assoc (sym, env->block.at (0));
	error ("unbound identifier", Atom::make_element (sym));
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
tail_call:
    if (is_null (node)) return Atom::make_sequence();
    AtomPtr params = Atom::make_sequence();
    for (unsigned i = 0; i < node->block.size (); ++i) {
    	AtomPtr c = node->block.at (i);
    	if (c->type == LIST) {
    		params->block.push_back(eval (c, env));
    	} else {
			if (c->token == "\n") continue;
			else if (c->token[0] == '$') {
				params->block.push_back(assoc (c->token.substr(1, c->token.size ()-1), env));
			}
    		else params->block.push_back(c);
    	}
    }
    if (params->block.size () == 0) return Atom::make_sequence();
    AtomPtr cmd = assoc (params->block.at (0)->token, env);
    params->block.pop_front ();
    if (cmd->type == PROC) {
		AtomPtr args = cmd->block.at (0)->block.at (0); // FIXME
		AtomPtr body = cmd->block.at (1);
		AtomPtr closure = is_null (cmd->block.at (2)) ? env : cmd->block.at (2);
		AtomPtr nenv = Atom::make_sequence ();
		nenv->block.push_back(closure);
		int minargs = args->block.size () < params->block.size () ? args->block.size () 
			: params->block.size ();
		for (unsigned i = 0; i < minargs; ++i) {
			extend (type_check(args->block.at (i), AtomType::ELEMENT), 
				params->block.at (i), nenv);
		}
		// partial functions
		if (args->block.size () > params->block.size ()) {
			AtomPtr args_cut = Atom::make_sequence ();
			for (unsigned i = minargs; i < args->block.size (); ++i) {
				args_cut->block.push_back (args->block.at (i));
			}
			return Atom::make_lambda (args_cut, body, nenv);
		}			
		for (unsigned i = 0; i < body->block.size () - 1; ++i) {
			eval (body->block.at (i), nenv);
		}
		env = nenv;
		node = body->block.at (body->block.size () - 1); // tail recursion
		goto tail_call;
    } else if (cmd->type == BUILTIN) {
    	args_check(params, cmd->minargs, node);
    	return cmd->func (params, env);
    } else error ("function expected in", cmd);
    return Atom::make_sequence();
}

// builtins
std::ostream& print (AtomPtr node, std::ostream& out) {
	switch (node->type) {
		case ELEMENT:
			out << node->token;
		break;
		case LIST: case COLL:
			out << (node->type == LIST ? "[" : "{");
			for (unsigned i = 0; i < node->block.size (); ++i) {
				print (node->block.at (i), out) << " ";
			}
			out << (node->type == LIST ? "]" : "}");
		break;
		case PROC:
			if (node->block.size () < 2) return out;
			print (node->block.at (0), out) << std::endl;
			print (node->block.at (1), out) << std::endl;
		break;
		case BUILTIN: 
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
    return extend (b->block.at (0), b->block.at (1), env);
}
template <bool dynamic>
AtomPtr fn_lambda (AtomPtr n, AtomPtr env) {
	return Atom::make_lambda (n->block.at (0), n->block.at (1), 
		dynamic ? Atom::make_sequence () : env);	
}
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
void add_func (const std::string& name, Builtin f, int minargs, AtomPtr env) {
	AtomPtr op = Atom::make_builtin (f, minargs); 
	op->token = name;
	extend (Atom::make_element (name), op, env);
}

AtomPtr make_env () {
	AtomPtr env = Atom::make_sequence ();
	env->block.push_back (Atom::make_sequence ()); // parent

    add_func("puts", fn_puts, 0, env);
    add_func("set", fn_set, 2, env);
    add_func("lambda", fn_lambda<false>, 2, env);
    // nspace.builtins["load"] = fn_load;
    // nspace.builtins["proc"] = fn_proc;
    add_func("+", fn_binop<'+'>, 2, env);
    add_func("-", fn_binop<'-'>, 2, env);
    add_func("*", fn_binop<'*'>, 2, env);
    add_func("/", fn_binop<'/'>, 2, env);
    add_func("<", fn_binop<'<'>, 2, env);
    add_func(">", fn_binop<'>'>, 2, env);
    add_func("<=", fn_binop<'L'>, 2, env);
    add_func(">=", fn_binop<'G'>, 2, env);
    // nspace.variables["nl"] = "\n";
    return env;
}
void repl (std::istream& in, AtomPtr env) {
    while (true) { 
        std::cout << ">> ";
        try {
            print (eval (read (in), env), std::cout) << std::endl;
        } catch (std::exception& err) {
            std::cout << "error: " << err.what () << std::endl;
        }
    }  
}

// EOF



#endif // SKICL_H	