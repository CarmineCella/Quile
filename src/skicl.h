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
#include <regex>

// TODO: primitive, if, eval, serializzazione, test
// speed, return, break, continue

// ast
struct Atom;
typedef std::shared_ptr<Atom> AtomPtr;
typedef double Real;
typedef AtomPtr (*Builtin) (AtomPtr, AtomPtr);
enum AtomType {NUMBER, SYMBOL, STRING, COLL, LIST, PROC, BUILTIN};
const char* TYPE_NAMES[] = {"number", "symbol", "string", "coll", "list", "proc",  "builtin"};
struct Atom {
private:
	// create only via factory methods
	struct _constructor_tag { explicit _constructor_tag() = default; }; 
public:	
	Atom (_constructor_tag) {
		 token = ""; func = nullptr; 
	}	
	AtomType type;
	Real value;
	std::string token;
	std::deque<AtomPtr> block;
	Builtin func;
	int minargs;
	static AtomPtr make_sequence (bool is_list = false) { 
		AtomPtr l = std::make_shared<Atom> (_constructor_tag{}); 
		l->type = is_list ? AtomType::LIST : AtomType::COLL;
		return l; 
	}
	static AtomPtr make_number (Real v) { 
		AtomPtr l = std::make_shared<Atom> (_constructor_tag{}); 
		l->type = AtomType::NUMBER;
		l->value = v;
		return l; 
	}	
	static AtomPtr make_symbol (const std::string& lex) { 
		static std::map<std::string, AtomPtr> dictionary;
		if (dictionary.find (lex) != dictionary.end ()) {
			return dictionary[lex];
		}
		else {
			AtomPtr s = std::make_shared<Atom> (_constructor_tag{}); 
			s->type = AtomType::SYMBOL;
			s->token = lex; 
			dictionary[lex] = s;
			return s;
		}
	}	
	static AtomPtr make_string (const std::string& s) { 
		AtomPtr l = std::make_shared<Atom> (_constructor_tag{}); 
		l->type = AtomType::STRING;
		l->token = s;
		return l; 
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
	    case AtomType::NUMBER: return (x->value == y->value);
    	case AtomType::SYMBOL: return (x == y);
    	case AtomType::STRING: return (x->token == y->token);
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
                if (c == '\n') input.unget();
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
std::ostream& puts (AtomPtr node, std::ostream& out);
void error (const std::string& err, AtomPtr node) {
	std::stringstream tmp;
	tmp << err;
	if (node) {
		std::stringstream tmp2;
		puts (node, tmp2);
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
AtomPtr type_check (AtomPtr node, AtomType type, AtomPtr ctx) {
	if (is_null (node)) return node;
	if (node->type != type) {
		std::stringstream err;
		err << "invalid type ";
		err << TYPE_NAMES[static_cast<typename 	std::underlying_type<AtomType>::type>(node->type)];
		err << " (expected " << TYPE_NAMES[static_cast<typename 
		std::underlying_type<AtomType>::type>(type)] << ") in";
		error (err.str (), ctx);
	}
	return node;
}
bool is_number (std::string token) {
    return std::regex_match(token, std::regex (("((\\+|-)?[[:digit:]]+)(\\.(([[:digit:]]+)?))?" )));
}
bool is_string (const std::string& s) {
	return s.find ("\"") != std::string::npos;
}
AtomPtr gets (std::istream& input, const std::string& terminator = "\n") {
	AtomPtr code = Atom::make_sequence (terminator != "}");
	while (!input.eof ()) {
		std::string token = get_token (input);
		if (token.size() == 0) continue;
		if (token == terminator) break;
		if (token == "[" || token == "{") {
			std::string valid_terminator = "]";
			if (token == "{") valid_terminator = "}"; 
			code->block.push_back(gets(input, valid_terminator));
		} else if (is_number (token)) {
			code->block.push_back(Atom::make_number(atof (token.c_str ())));
		} else if (is_string (token)) {
			code->block.push_back(Atom::make_string(token.substr(1, token.size () - 2)));
		} else {
			code->block.push_back(Atom::make_symbol (token));
		}
	}
	return code;
}
AtomPtr assoc (const std::string& sym, AtomPtr env) {
	AtomPtr r = Atom::make_sequence ();;
	for (unsigned i = 1; i < env->block.size () / 2 + 1; ++i) {
		if (env->block.at (i * 2 - 1)->token == sym) {
			return env->block.at (i * 2);
		}
	}
	if (!is_null (env->block.at (0))) return assoc (sym, env->block.at (0));
	error ("unbound identifier", Atom::make_symbol (sym));
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
		else error ("unbound identifier", key);
	} 
	env->block.push_back (key); env->block.push_back (val);
	return val;
}
AtomPtr split_sequence (AtomPtr ct) {
	AtomPtr outer = Atom::make_sequence(ct->type == LIST);
	AtomPtr inner = Atom::make_sequence(true); // always executable
	for (unsigned t = 0; t < ct->block.size ();  ++t) {
		if (ct->block.at(t)->token == "\n") {
			if (!is_null (inner)) outer->block.push_back(inner);
			inner = Atom::make_sequence(true);
		} else inner->block.push_back(ct->block.at(t));
	}
	if (!is_null (inner)) outer->block.push_back(inner);
	return outer;
}
AtomPtr fn_eval (AtomPtr b,  AtomPtr env) { return nullptr; } // dummy
AtomPtr fn_if (AtomPtr b,  AtomPtr env) { return nullptr; } // dummy
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
    AtomPtr cmd = assoc (type_check(params->block.at (0), AtomType::SYMBOL, node)->token, env);
    params->block.pop_front ();
    if (cmd->type == PROC) {
		AtomPtr args = cmd->block.at (0);
		AtomPtr code = cmd->block.at (1);
		AtomPtr closure = is_null (cmd->block.at (2)) ? env : cmd->block.at (2);
		AtomPtr nenv = Atom::make_sequence ();
		nenv->block.push_back(closure);	
		int minargs = args->block.size () < params->block.size () ? args->block.size () 
			: params->block.size ();
		for (unsigned i = 0; i < minargs; ++i) {
			extend (args->block.at (i), params->block.at (i), nenv);
		}
		// partial functions
		if (args->block.size () > params->block.size ()) {
			AtomPtr args_cut = Atom::make_sequence ();
			for (unsigned i = minargs; i < args->block.size (); ++i) {
				args_cut->block.push_back (args->block.at (i));
			}
			return Atom::make_lambda (args_cut, code, nenv);
		}			
		// variable arguments
		AtomPtr varargs = Atom::make_sequence();
		for (unsigned i = args->block.size (); i < params->block.size (); ++i) {
			varargs->block.push_back (params->block.at(i));
		} 		
		extend (Atom::make_symbol("&"), varargs, nenv);
		for (unsigned i = 0; i < code->block.size () - 1; ++i) {
			eval (code->block.at (i), nenv);
		}
		env = nenv;
		node = code->block.at (code->block.size () - 1); // tail recursion
		goto tail_call;
    } else if (cmd->type == BUILTIN) {
    	args_check(params, cmd->minargs, node);
    	if (cmd->func == &fn_if) {
    		AtomPtr code = split_sequence (params->block.at (1));
    		if (type_check (eval (params->block.at (0), env), 
    			AtomType::NUMBER, node)->value) {
				for (unsigned i = 0; i < code->block.size () - 1; ++i) {
					eval (code->block.at (i), env);
				}
				node = code->block.at (code->block.size () - 1); // tail recursion
				goto tail_call;
			} else return Atom::make_sequence();		
		} else if (cmd->func == &fn_eval) {
    		AtomPtr code = split_sequence (params->block.at (0));
			for (unsigned i = 0; i < code->block.size () - 1; ++i) {
				eval (code->block.at (i), env);
			}
			node = code->block.at (code->block.size () - 1); // tail recursion
			goto tail_call;
		}
    	return cmd->func (params, env);
    } else error ("function expected in", cmd);
    return Atom::make_sequence();
}

// builtins
std::ostream& puts (AtomPtr node, std::ostream& out) {
	switch (node->type) {
		case NUMBER:
			out << node->value;
		break;
		case SYMBOL: case STRING:
			out << node->token;
		break;
		case LIST: case COLL:
			out << (node->type == LIST ? "[" : "{");
			for (unsigned i = 0; i < node->block.size (); ++i) {
				puts (node->block.at (i), out) << " ";
			}
			out << (node->type == LIST ? "]" : "}");
		break;
		case PROC:
			if (node->block.size () < 3) return out;
			if (is_null (node->block.at (2))) out << "<dynamic> ";
			else out << "<static> ";
			puts (node->block.at (0), out) << " ";
			puts (node->block.at (1), out) << std::endl;
		break;
		case BUILTIN: 
			out << "<" << (std::hex) << &node->func << ", min args: " 
				<< node->minargs << ">";
		break;
	}
	return out;
}
AtomPtr fn_puts (AtomPtr b, AtomPtr env) {
    for (unsigned i = 0; i < b->block.size (); ++i) {
    	puts (b->block.at (i), std::cout);
    }
    return Atom::make_symbol("");
}
template <bool recurse>
AtomPtr fn_set (AtomPtr b,  AtomPtr env) {
	AtomPtr res = Atom::make_sequence();
    for (unsigned i  = 0; i < b->block.size () / 2; ++i) {
    	res = extend (type_check (b->block.at (2 * i), AtomType::SYMBOL, b), 
    		b->block.at (2 * i + 1), env, recurse);
    }
    return res;
}
AtomPtr fn_updef (AtomPtr b,  AtomPtr env) {
	if (is_null(env->block.at (0))) error ("cannot use updef in outer environment", env);
	return extend (type_check (b->block.at (0), AtomType::SYMBOL, b), 
		b->block.at (1), env->block.at (0));
}
template <bool dynamic>
AtomPtr fn_lambda (AtomPtr n, AtomPtr env) {
	// AtomPtr closure = Atom::make_sequence();
	// *closure = *env;
	return Atom::make_lambda (type_check (n->block.at (0), AtomType::COLL, n), 
		split_sequence (type_check (n->block.at (1), AtomType::COLL, n)),
		dynamic ? Atom::make_sequence () : env);	
}
AtomPtr fn_while (AtomPtr b,  AtomPtr env) {
	AtomPtr res = Atom::make_sequence();
	AtomPtr code = split_sequence (type_check (b->block.at (1), AtomType::COLL, b));
	while (type_check (eval (b->block. at (0), env), AtomType::NUMBER, b)->value) {
		for (unsigned i = 0; i < code->block.size (); ++i) {
			eval (code->block.at (i), env);
		}
	}
	return res;
}
template <char op>
AtomPtr fn_binop (AtomPtr b,  AtomPtr env) {
    Real val = 0;
    Real first = type_check (b->block.at (0), AtomType::NUMBER, b)->value; 
    Real second = type_check (b->block.at (1), AtomType::NUMBER, b)->value;
    switch (op) {
        case '+':  val = first + second;  break;
        case '-':  val = first - second;  break;
        case '*':  val = first * second;  break;
        case '/':  val =first / second;  break;
        case '<':  val = (first < second);  break;
        case '>':  val = (first > second);  break;
        case 'L':  val = (first <= second);  break;
        case 'G':  val = (first >= second);  break;
        case '=':  val = (first == second);  break;
    }
    return Atom::make_number(val);
}
AtomPtr source (const std::string& name, AtomPtr env) {
	std::ifstream in (name.c_str ());
	if (!in.good ()) {
		std::string longname = getenv("HOME");
		longname += "/.skicl/" + name;
		in.open (longname.c_str());
		if (!in.good ()) return Atom::make_symbol("0");
	}
    AtomPtr res;
	while (!in.eof ()) {
		res = eval (gets (in), env);
	}
	return res;
}
AtomPtr fn_source (AtomPtr b,  AtomPtr env) {
    return source (b->block.at (0)->token, env);
}

// interface
void add_builtin (const std::string& name, Builtin f, int minargs, AtomPtr env) {
	AtomPtr op = Atom::make_builtin (f, minargs); 
	op->token = name;
	extend (Atom::make_symbol (name), op, env);
}
AtomPtr make_env () {
	AtomPtr env = Atom::make_sequence ();
	env->block.push_back (Atom::make_sequence ()); // parent
    add_builtin("puts", fn_puts, 1, env);
    add_builtin("set", fn_set<false>, 2, env);
    add_builtin("setrec", fn_set<true>, 2, env);
    add_builtin("updef", fn_updef, 2, env);
    add_builtin("eval", fn_eval, 1, env);
    add_builtin("if", fn_if, 2, env);
    add_builtin("while", fn_while, 2, env);
    add_builtin("\\", fn_lambda<false>, 2, env);
    add_builtin("@", fn_lambda<true>, 2, env);
    add_builtin("source", fn_source, 1, env);
    add_builtin("+", fn_binop<'+'>, 2, env);
    add_builtin("-", fn_binop<'-'>, 2, env);
    add_builtin("*", fn_binop<'*'>, 2, env);
    add_builtin("/", fn_binop<'/'>, 2, env);
    add_builtin("<", fn_binop<'<'>, 2, env);
    add_builtin(">", fn_binop<'>'>, 2, env);
    add_builtin("<=", fn_binop<'L'>, 2, env);
    add_builtin(">=", fn_binop<'G'>, 2, env);
    extend(Atom::make_symbol("nl"), Atom::make_symbol("\n"), env);
    return env;
}
void repl (std::istream& in, AtomPtr env) {
    while (true) { 
        std::cout << "% ";
        try {
            puts (eval (gets (in), env), std::cout) << std::endl;
            // puts (gets (in), std::cout) << std::endl;
        } catch (std::exception& err) {
            std::cout << "error: " << err.what () << std::endl;
        }
    }  
}

// EOF

#endif // SKICL_H	