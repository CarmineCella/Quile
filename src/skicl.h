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

// TODO: primitive, serializzazione, test speed, core lib

// ast
struct Atom;
typedef std::shared_ptr<Atom> AtomPtr;
typedef double Real;
typedef AtomPtr (*Builtin) (AtomPtr, AtomPtr);
enum AtomType {NUMBER, SYMBOL, STRING, LIST, STREAM, PROC, BUILTIN};
const char* TYPE_NAMES[] = {"number", "symbol", "string", "list", "stream", "proc",  "builtin"};
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
	std::deque<AtomPtr> sequence;
	Builtin func;
	int minargs;
	static AtomPtr make_sequence (bool is_stream = false) { 
		AtomPtr l = std::make_shared<Atom> (_constructor_tag{}); 
		l->type = is_stream ? AtomType::STREAM : AtomType::LIST;
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
		s->sequence.push_back(args);
		s->sequence.push_back(body);
		s->sequence.push_back(closure);
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
		((node->type == AtomType::LIST || node->type == AtomType::STREAM) 
			&& node->sequence.size () == 0);
}
int atom_eq (AtomPtr x, AtomPtr y) {
	if (x->type != y->type) return 0;
	switch (x->type) {
	    case AtomType::NUMBER: return (x->value == y->value);
    	case AtomType::SYMBOL: return (x == y);
    	case AtomType::STRING: return (x->token == y->token);
	    case AtomType::LIST: case AtomType::STREAM:  {
			if (x->sequence.size () != y->sequence.size ()) { return 0; }
			for (unsigned i = 0; i < x->sequence.size (); ++i) {
				if (!atom_eq (x->sequence.at(i), y->sequence.at(i))) { return 0; }
			}
			return 1;
		}
		case AtomType::PROC: 
			if (x->sequence.size () < 2 || y->sequence.size () < 2) return 0;
			return (atom_eq (x->sequence.at(0), y->sequence.at(0)) 
				&& atom_eq (x->sequence.at(1), y->sequence.at(1)));
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
	if (node->sequence.size () < ct) {
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
AtomPtr gets (std::istream& input, int& nesting, const std::string& terminator) {
	AtomPtr code = Atom::make_sequence (terminator != "}");
	while (!input.eof ()) {
		std::string token = get_token (input);
		if (token.size() == 0) continue;
		if (token == terminator) {
			--nesting;
			break;
		}
		if (token == "[" || token == "{") {
			++nesting;
			std::string valid_terminator = "]";
			if (token == "{") valid_terminator = "}"; 
			code->sequence.push_back(gets(input, nesting, valid_terminator));
		} else if (is_number (token)) {
			code->sequence.push_back(Atom::make_number(atof (token.c_str ())));
		} else if (is_string (token)) {
			code->sequence.push_back(Atom::make_string(token.substr(1, token.size () - 2)));
		} else {
			code->sequence.push_back(Atom::make_symbol (token));
		}
	}
	return code;
}
AtomPtr gets (std::istream& input) {
	int nesting = 1;
	AtomPtr r = gets (input, nesting, "\n");
	if (nesting && !is_null (r)) error ("invalid nesting in", r);
	return r;
}
AtomPtr assoc (const std::string& sym, AtomPtr env) {
	AtomPtr r = Atom::make_sequence ();;
	for (unsigned i = 1; i < env->sequence.size () / 2 + 1; ++i) {
		if (env->sequence.at (i * 2 - 1)->token == sym) {
			return env->sequence.at (i * 2);
		}
	}
	if (!is_null (env->sequence.at (0))) return assoc (sym, env->sequence.at (0));
	error ("unbound identifier", Atom::make_symbol (sym));
	return Atom::make_sequence ();; // not reached
}
AtomPtr extend (AtomPtr key, AtomPtr val, AtomPtr env, bool recurse = false) {
	for (unsigned i = 1; i <= env->sequence.size () / 2; ++i) {
		if (atom_eq (env->sequence.at (2 * i - 1), key)) {
			env->sequence.at (2 * i) = val;
			return val;
		}
	}
	if (recurse) {
		if (!is_null(env->sequence.at(0))) return extend(key, val, env->sequence.at(0), recurse);
		else error ("unbound identifier", key);
	} 
	env->sequence.push_back (key); env->sequence.push_back (val);
	return val;
}
AtomPtr split_sequence (AtomPtr ct) {
	AtomPtr outer = Atom::make_sequence(ct->type == STREAM);
	AtomPtr inner = Atom::make_sequence(true); // always executable
	for (unsigned t = 0; t < ct->sequence.size ();  ++t) {
		if (ct->sequence.at(t)->token == "\n") {
			if (!is_null (inner)) outer->sequence.push_back(inner);
			inner = Atom::make_sequence(true);
		} else inner->sequence.push_back(ct->sequence.at(t));
	}
	if (!is_null (inner)) outer->sequence.push_back(inner);
	return outer;
}
AtomPtr fn_eval (AtomPtr b,  AtomPtr env) { return nullptr; } // dummy
AtomPtr fn_if (AtomPtr b,  AtomPtr env) { return nullptr; } // dummy
AtomPtr eval (AtomPtr node, AtomPtr env) {
tail_call:
    if (is_null (node)) return Atom::make_sequence();
    AtomPtr params = Atom::make_sequence();
    for (unsigned i = 0; i < node->sequence.size (); ++i) {
    	AtomPtr c = node->sequence.at (i);
    	if (c->type == STREAM) {
    		params->sequence.push_back(eval (c, env));
    	} else {
			if (c->token == "\n") continue;
			else if (c->token[0] == '$') {
				params->sequence.push_back(assoc (c->token.substr(1, c->token.size ()-1), env));
			}
    		else params->sequence.push_back(c);
    	}
    }

    if (params->sequence.size () == 0) return Atom::make_sequence();
    AtomPtr cmd = params->sequence.at (0);
    if (cmd->type == AtomType::SYMBOL) cmd = assoc (cmd->token, env);

    params->sequence.pop_front ();
    if (cmd->type == PROC) {
		AtomPtr args = cmd->sequence.at (0);
		AtomPtr code = cmd->sequence.at (1);
		AtomPtr closure = is_null (cmd->sequence.at (2)) ? env : cmd->sequence.at (2);
		AtomPtr nenv = Atom::make_sequence ();
		nenv->sequence.push_back(closure);	
		int minargs = args->sequence.size () < params->sequence.size () ? args->sequence.size () 
			: params->sequence.size ();
		for (unsigned i = 0; i < minargs; ++i) {
			extend (args->sequence.at (i), params->sequence.at (i), nenv);
		}
		// partial functions
		if (args->sequence.size () > params->sequence.size ()) {
			AtomPtr args_cut = Atom::make_sequence ();
			for (unsigned i = minargs; i < args->sequence.size (); ++i) {
				args_cut->sequence.push_back (args->sequence.at (i));
			}
			return Atom::make_lambda (args_cut, code, nenv);
		}			
		// variable arguments
		AtomPtr varargs = Atom::make_sequence();
		for (unsigned i = args->sequence.size (); i < params->sequence.size (); ++i) {
			varargs->sequence.push_back (params->sequence.at(i));
		} 		
		extend (Atom::make_symbol("&"), varargs, nenv);
		for (unsigned i = 0; i < code->sequence.size () - 1; ++i) {
			eval (code->sequence.at (i), nenv);
		}
		env = nenv;
		node = code->sequence.at (code->sequence.size () - 1); // tail recursion
		goto tail_call;
    } else if (cmd->type == BUILTIN) {
    	args_check(params, cmd->minargs, node);
    	if (cmd->func == &fn_if) {
    		bool has_else = false;
			if (params->sequence.size () >  2) {
				if  (params->sequence.at (2)->token != "else" || params->sequence.size () != 4) {
					error ("if/else syntax error in ", node);
				}    		
				has_else = true;
			}
    		AtomPtr code = split_sequence (params->sequence.at (1));
    		if (type_check (eval (params->sequence.at (0), env), 
    			AtomType::NUMBER, node)->value) {
				for (unsigned i = 0; i < code->sequence.size () - 1; ++i) {
					eval (code->sequence.at (i), env);
				}
				node = code->sequence.at (code->sequence.size () - 1); // tail recursion
				goto tail_call;
			} else {
				if (has_else) {
					code = split_sequence (params->sequence.at (3));
					for (unsigned i = 0; i < code->sequence.size () - 1; ++i) {
						eval (code->sequence.at (i), env);
					}
					node = code->sequence.at (code->sequence.size () - 1); // tail recursion
					goto tail_call;					
				} else return Atom::make_sequence();		
			}
		} else if (cmd->func == &fn_eval) {
    		AtomPtr code = split_sequence (params->sequence.at (0));
			for (unsigned i = 0; i < code->sequence.size () - 1; ++i) {
				eval (code->sequence.at (i), env);
			}
			node = code->sequence.at (code->sequence.size () - 1); // tail recursion
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
		case STREAM: case LIST:
			out << (node->type == STREAM ? "[" : "{");
			for (unsigned i = 0; i < node->sequence.size (); ++i) {
				puts (node->sequence.at (i), out) << " ";
			}
			out << (node->type == STREAM ? "]" : "}");
		break;
		case PROC:
			if (node->sequence.size () < 3) return out;
			if (is_null (node->sequence.at (2))) out << "<dynamic> ";
			else out << "<static> ";
			puts (node->sequence.at (0), out) << " ";
			puts (node->sequence.at (1), out) << std::endl;
		break;
		case BUILTIN: 
			out << "<" << (std::hex) << &node->func << ", min args: " 
				<< node->minargs << ">";
		break;
	}
	return out;
}
AtomPtr fn_puts (AtomPtr b, AtomPtr env) {
    for (unsigned i = 0; i < b->sequence.size (); ++i) {
    	puts (b->sequence.at (i), std::cout);
    }
    return Atom::make_symbol("");
}
template <bool recurse>
AtomPtr fn_set (AtomPtr b,  AtomPtr env) {
	AtomPtr res = Atom::make_sequence();
    for (unsigned i  = 0; i < b->sequence.size () / 2; ++i) {
    	res = extend (type_check (b->sequence.at (2 * i), AtomType::SYMBOL, b), 
    		b->sequence.at (2 * i + 1), env, recurse);
    }
    return res;
}
AtomPtr fn_updef (AtomPtr b,  AtomPtr env) {
	if (is_null(env->sequence.at (0))) error ("cannot use updef in outer environment", env);
	return extend (type_check (b->sequence.at (0), AtomType::SYMBOL, b), 
		b->sequence.at (1), env->sequence.at (0));
}
template <bool dynamic>
AtomPtr fn_lambda (AtomPtr n, AtomPtr env) {
	// AtomPtr closure = Atom::make_sequence();
	// *closure = *env;
	return Atom::make_lambda (type_check (n->sequence.at (0), AtomType::LIST, n), 
		split_sequence (type_check (n->sequence.at (1), AtomType::LIST, n)),
		dynamic ? Atom::make_sequence () : env);	
}
AtomPtr fn_list (AtomPtr node, AtomPtr env) {
	return node;
}
AtomPtr join (AtomPtr dst, AtomPtr ll) {
	if (ll->type == AtomType::LIST) {
		for (unsigned i = 0; i < ll->sequence.size (); ++i) {
			dst->sequence.push_back (ll->sequence.at (i));
		}
	} else dst->sequence.push_back (ll);
	return dst;
}
AtomPtr fn_join (AtomPtr n, AtomPtr env) {
	AtomPtr dst = n->sequence.at(0);
	for (unsigned i = 1; i < n->sequence.size (); ++i){
		dst = join (dst, n->sequence.at(i));
	}	
	return dst;
}
AtomPtr fn_lindex (AtomPtr params, AtomPtr env) {
	AtomPtr l = type_check (params->sequence.at (0), AtomType::LIST, params);
	int i = (int) (type_check(params->sequence.at (1), AtomType::NUMBER, params)->value);
	if (i < 0 || i >= l->sequence.size ()) return Atom::make_sequence();
	return l->sequence.at(i);
}
AtomPtr fn_lrange (AtomPtr params, AtomPtr env) {
	AtomPtr l = type_check (params->sequence.at (0), AtomType::LIST, params);
	int i = (int) (type_check(params->sequence.at (1), AtomType::NUMBER, params)->value);
	int e = (int) (type_check(params->sequence.at (2), AtomType::NUMBER, params)->value);
	if (i < 0 || i >= l->sequence.size () || e < 0 || e >= l->sequence.size () || e < i) {
		return Atom::make_sequence();
	}
	AtomPtr nl = Atom::make_sequence();
	for (int j = i; j <= e; ++j) nl->sequence.push_back(l->sequence.at (j));
	return nl;
}
AtomPtr fn_llength (AtomPtr params, AtomPtr env) {
	AtomPtr l = type_check (params->sequence.at (0), AtomType::LIST, params);
	return Atom::make_number (l->sequence.size ());
}
AtomPtr gets_fn (AtomPtr node, AtomPtr env) {
	return gets (std::cin);
}
AtomPtr fn_while (AtomPtr b,  AtomPtr env) {
	AtomPtr res = Atom::make_sequence();
	AtomPtr code = split_sequence (type_check (b->sequence.at (1), AtomType::LIST, b));
	while (type_check (eval (b->sequence. at (0), env), AtomType::NUMBER, b)->value) {
		for (unsigned i = 0; i < code->sequence.size (); ++i) {
			res = eval (code->sequence.at (i), env);
			if (res->token == "continue") ++i;
			if (res->token == "break"){		
				return res;	
			} 				
		}
	}
	return res;
}
template <int command>
AtomPtr fn_passby (AtomPtr b,  AtomPtr env) {
	switch (command) {
		case 0:
			return Atom::make_symbol("continue");
		break;
		case 1:
			return Atom::make_symbol("break");
		break;
	}
	return Atom::make_sequence();
}
template <char op>
AtomPtr fn_binop (AtomPtr b,  AtomPtr env) {
    Real val = 0;
    Real first = type_check (b->sequence.at (0), AtomType::NUMBER, b)->value; 
    Real second = type_check (b->sequence.at (1), AtomType::NUMBER, b)->value;
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
AtomPtr fn_eq (AtomPtr n, AtomPtr env) {
	return Atom::make_number(atom_eq (n->sequence.at(0), n->sequence.at(1)));
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
    return source (b->sequence.at (0)->token, env);
}

// interface
void add_builtin (const std::string& name, Builtin f, int minargs, AtomPtr env) {
	AtomPtr op = Atom::make_builtin (f, minargs); 
	op->token = name;
	extend (Atom::make_symbol (name), op, env);
}
AtomPtr make_env () {
	AtomPtr env = Atom::make_sequence ();
	env->sequence.push_back (Atom::make_sequence ()); // parent
    // environments
    add_builtin("set", fn_set<false>, 2, env);
    add_builtin("setrec", fn_set<true>, 2, env);
    add_builtin("updef", fn_updef, 2, env);
    add_builtin("\\", fn_lambda<false>, 2, env);
    add_builtin("@", fn_lambda<true>, 2, env);
    // flow control
    add_builtin("eval", fn_eval, 1, env);
    add_builtin("if", fn_if, 2, env);
    add_builtin("while", fn_while, 2, env);
    add_builtin("continue", fn_passby<0>, 0, env);
    add_builtin("break", fn_passby<1>, 0, env);
	// sequences
	add_builtin ("list", fn_list, 0, env);
	add_builtin ("join", fn_join, 1, env);
	add_builtin ("lindex", fn_lindex, 1, env);
	add_builtin ("lrange", fn_lrange, 2, env);
	add_builtin ("llength", fn_llength, 1, env);
    //I/O
    add_builtin("puts", fn_puts, 1, env);
    add_builtin("source", fn_source, 1, env);
    extend(Atom::make_symbol("nl"), Atom::make_symbol("\n"), env);
    // arithmetics/logic
    add_builtin("+", fn_binop<'+'>, 2, env);
    add_builtin("-", fn_binop<'-'>, 2, env);
    add_builtin("*", fn_binop<'*'>, 2, env);
    add_builtin("/", fn_binop<'/'>, 2, env);
    add_builtin("<", fn_binop<'<'>, 2, env);
    add_builtin(">", fn_binop<'>'>, 2, env);
    add_builtin("<=", fn_binop<'L'>, 2, env);
    add_builtin(">=", fn_binop<'G'>, 2, env);
    add_builtin("eq", fn_eq, 2, env);
    //others
    return env;
}
void repl (std::istream& in, AtomPtr env) {
    while (true) { 
        std::cout << "% ";
        try {
            puts (eval (gets (in), env), std::cout) << std::endl;
            // puts (gets (in, nesting), std::cout) << std::endl;
        } catch (std::exception& err) {
            std::cout << "error: " << err.what () << std::endl;
        }
    }  
}

// EOF

#endif // SKICL_H	