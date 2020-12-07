// quile.h
//
#ifndef QUILE_H
#define QUILE_H

#include <deque>
#include <string>
#include <sstream>
#include <stdexcept>
#include <fstream>
#include <memory>
#include <iostream>
#include <map>
#include <regex>
#include <cmath>
#include <dlfcn.h>
#if defined (ENABLE_READLINE)
	#include <readline/readline.h>
	#include <readline/history.h>
#endif

#define BOLDBLUE    "\033[1m\033[34m"
#define RED     	"\033[31m" 
#define RESET   	"\033[0m"

// TODO: line numbers, tests, libraries, deque bug for eval and if, lrange with stride, linterleave
// ast
struct Atom;
typedef std::shared_ptr<Atom> AtomPtr;
typedef double Real;
typedef AtomPtr (*Builtin) (AtomPtr, AtomPtr);
enum AtomType {NUMBER, SYMBOL, STRING, LIST, STREAM, PROC, BUILTIN, OBJECT};
const char* TYPE_NAMES[] = {"number", "symbol", "string", "list", "stream", "proc", "builtin", "object"};
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
	void* obj;
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
	static AtomPtr make_object (const std::string& type, void * o, AtomPtr cb) { 
		AtomPtr p = std::make_shared<Atom> (_constructor_tag{}); 
		p->type = AtomType::OBJECT;
		p->obj = o; p->token = type; p->sequence.push_back (cb);
		return p;
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
		case AtomType::OBJECT: return (x->token == y->token && x->obj == y->obj);	    
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
				return accum.str (); //.substr(1, accum.str ().size ()  - 2); 
			break;             
			default:
				if (c > 0) accum << c;
		}
	}
	return accum.str ();
}

// parsing and evaluation
std::ostream& puts (AtomPtr node, std::ostream& out, bool is_write);
void error (const std::string& err, AtomPtr node) {
	std::stringstream tmp;
	tmp << err;
	if (node) {
		std::stringstream tmp2;
		puts (node, tmp2, false);
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
    return std::regex_match(token, std::regex (("((\\+|-)?[[:digit:]]+)(\\.(([[:digit:]]+)?))?")));
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
	if (nesting && !is_null (r)) error ("syntax error (missing newline/invalid nesting) in", r);
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
				if (c->token.size () == 1) error ("missing variable name in", node);
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
					error ("invalid if/else syntax in ", node);
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
std::ostream& puts (AtomPtr node, std::ostream& out, bool is_write = false) {
	switch (node->type) {
		case NUMBER:
			out << node->value;
		break;
		case SYMBOL: case STRING:
			if (node->type == STRING && is_write ) out << "\"" << node->token << "\"";
			else out << node->token;
		break;
		case STREAM: case LIST:
			out << (node->type == STREAM ? "[" : "{");
			for (unsigned i = 0; i < node->sequence.size (); ++i) {
				puts (node->sequence.at (i), out, is_write) << 
					(i == node->sequence.size () - 1 ? "" : " ");
			}
			out << (node->type == STREAM ? "]" : "}");
		break;
		case PROC:
			if (node->sequence.size () < 3) return out;
			if (is_null (node->sequence.at (2))) out << "[@ ";
			else out << "[\\ "; 
			puts (node->sequence.at (0), out, is_write) << " ";
			puts (node->sequence.at (1), out, is_write) << "]";
		break;
		case BUILTIN: 
			if (is_write) {
				out << node->token;
			} else {
				out << "<" << (std::hex) << &node->func << ", min args: " 
					<< node->minargs << ">";
			}
		break;
		case OBJECT:
			out << "<object: " << node->token << ", " << &node->obj << ">";
		break;		
	}
	return out;
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
bool unset (AtomPtr k, AtomPtr env) {
	for (std::deque<AtomPtr>::iterator it = env->sequence.begin(); it != env->sequence.end (); ++it) {
		if (*it == k) {
			env->sequence.erase(it);
			env->sequence.erase(it);
			return true;
		}
	}
	if (!is_null(env->sequence.at(0))) return unset(k, env->sequence.at(0));
	return false;
}
AtomPtr fn_unset (AtomPtr b,  AtomPtr env) {
	for (unsigned i = 0; i < b->sequence.size (); ++i) {
		if (!unset(b->sequence.at (i), env)) {
			return Atom::make_number(0);
		}	
	}
	return Atom::make_number(1);
}
template <bool dynamic>
AtomPtr fn_lambda (AtomPtr n, AtomPtr env) {
	return Atom::make_lambda (type_check (n->sequence.at (0), AtomType::LIST, n), 
		split_sequence (type_check (n->sequence.at (1), AtomType::LIST, n)),
		dynamic ? Atom::make_sequence () : env);	
}
AtomPtr fn_info (AtomPtr b, AtomPtr env) {
	std::string cmd = b->sequence.at (0)->token;
	AtomPtr l = Atom::make_sequence();
	std::regex r;
	if (cmd == "vars") {
		if (b->sequence.size () > 1) {
			r.assign (b->sequence.at(1)->token);
		} else r.assign (".*");
		for (unsigned i = 1; i <= env->sequence.size () / 2; ++i) {
			std::string k = env->sequence.at (2 * i - 1)->token;
			if (std::regex_match(k, r)) {
				l->sequence.push_back(Atom::make_symbol(k));
			}
		}
    } else if (cmd == "exists") {
    	for (unsigned i = 1; i < b->sequence.size (); ++i) {
			std::string key = b->sequence.at (i)->token;		
			Real ans = 1;
			try {
				AtomPtr r = assoc (key, env);
			} catch (...) {
				ans = 0;
			}    	
			l->sequence.push_back(Atom::make_number(ans));
		}
	} else if (cmd == "typeof") {
    	for (unsigned i = 1; i < b->sequence.size (); ++i) {
			l->sequence.push_back(Atom::make_symbol(TYPE_NAMES[b->sequence.at (i)->type]));
		}
	} else {
		error ("invalid info request", b->sequence.at (0));
	}
    return l;
}
AtomPtr fn_list (AtomPtr node, AtomPtr env) {
	return node;
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
AtomPtr fn_ljoin (AtomPtr params, AtomPtr env) {
	AtomPtr dst = type_check (params->sequence.at (0), AtomType::LIST, params);
	for (unsigned i = 1; i < params->sequence.size (); ++i) {
		AtomPtr ll = params->sequence.at (i);
		if (ll->type == AtomType::LIST) {
			for (unsigned i = 0; i < ll->sequence.size (); ++i) {
				dst->sequence.push_back (ll->sequence.at (i));
			}
		} else dst->sequence.push_back (ll);
	}
	return dst;
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
		case 2:
			return b->sequence.at (0);
		break;
	}
	return Atom::make_sequence();
}
AtomPtr fn_throw (AtomPtr node, AtomPtr env) {
	// if (!node->sequence.at (0)->sequence.size ()) return Atom::make_sequence();
	throw node->sequence.at (0);
	return Atom::make_sequence ();;
}
AtomPtr fn_catch (AtomPtr node, AtomPtr env) { // not tail recursive
	AtomPtr sig = node->sequence.at(0);
	AtomPtr body = node->sequence.at(1);
	AtomPtr catcher = node->sequence.at (2);
	AtomPtr res = Atom::make_sequence ();
	try {
 		res = eval (body, env);
	} catch (AtomPtr& e) {
		if (atom_eq (e, sig)) {
			res = eval (catcher, env);
		} else throw e;
	}
	return res;
}
template <int mode>
AtomPtr fn_format (AtomPtr node, AtomPtr env) {
	std::stringstream tmp;
	for (unsigned i = (mode == 2 ? 1 : 0); i < node->sequence.size (); ++i) {
		puts (node->sequence.at (i), tmp, (mode == 2 ? true : false));
	}
	switch (mode) {
		case 0:
			std::cout << tmp.str ();
			return Atom::make_string ("");
		break;
		case 1:
			return Atom::make_string (tmp.str ());
		break;
		case 2:
			std::string fname = type_check (node->sequence.at (0), AtomType::STRING, node)->token;
			std::ofstream out (fname);
			if (!out.good ()) return Atom::make_sequence ();
			out << tmp.str ();
			out.close ();
			return Atom::make_number(1);
		break;	
	}
}
AtomPtr fn_gets(AtomPtr node, AtomPtr env) {
	if (node->sequence.size ()) {
		std::stringstream strtmp;
		strtmp << type_check(node->sequence.at (0), AtomType::STRING, node)->token << std::endl;
		return gets (strtmp);
	}
	else return gets (std::cin);
}
AtomPtr source (const std::string& name, AtomPtr env) {
	std::ifstream in (name.c_str ());
	if (!in.good ()) {
		std::string longname = getenv("HOME");
		longname += "/.quile/" + name;
		in.open (longname.c_str());
		if (!in.good ()) {
			error ("cannot open input file", Atom::make_string (name));
		}
	}
    AtomPtr res;
	while (!in.eof ()) {
		res = eval (gets (in), env);
	}
	return res;
}
AtomPtr fn_source (AtomPtr b,  AtomPtr env) {
    return source (type_check (b->sequence.at (0), AtomType::STRING, b)->token, env);
}
#define MAKE_BINOP(op,name, unit)									\
	AtomPtr name (AtomPtr n, AtomPtr env) {						\
		if (n->sequence.size () == 1) {										\
			return Atom::make_number(unit							\
				op (type_check (n->sequence.at(0), AtomType::NUMBER, n)->value));								\
		}															\
		AtomPtr c = n;												\
		Real s = (type_check (n->sequence.at(0), AtomType::NUMBER, n)->value);									\
		for (unsigned i = 1; i < n->sequence.size (); ++i) {					\
			s = s op type_check (n->sequence.at(i), AtomType::NUMBER, n)->value;								\
		}															\
		return Atom::make_number (s);								\
	}			

#define MAKE_CMPOP(op,fn_name) \
	AtomPtr fn_name (AtomPtr n, AtomPtr env) { \
		Real base = 1; \
		for (unsigned i = 0; i < n->sequence.size () - 1; ++i) { \
			if (!(type_check (n->sequence.at(i), AtomType::NUMBER, n)->value op \
				type_check (n->sequence.at(i + 1), AtomType::NUMBER, n)->value)) { \
				base = 0; \
			} \
		} \
		return Atom::make_number (base); \
	} \

#define MAKE_SINGOP(op,fn_name) \
	AtomPtr fn_name (AtomPtr n, AtomPtr env) { \
		return Atom::make_number (op(type_check (n->sequence.at(0), AtomType::NUMBER, n)->value)); \
	} \

MAKE_BINOP(+,fn_add,0);
MAKE_BINOP(-,fn_sub,0);
MAKE_BINOP(*,fn_mul,1);
MAKE_BINOP(/,fn_div,1);
MAKE_CMPOP(<,fn_less);
MAKE_CMPOP(>,fn_gt);
MAKE_CMPOP(<=,fn_lesseq);
MAKE_CMPOP(>=,fn_gteq);
MAKE_SINGOP(sin, fn_sin);
MAKE_SINGOP(cos,fn_cos);
MAKE_SINGOP(log, fn_log);
MAKE_SINGOP(exp, fn_exp);
MAKE_SINGOP(fabs, fn_fabs);
MAKE_SINGOP (sqrt,fn_sqrt);
MAKE_SINGOP (floor, fn_floor);
AtomPtr fn_eq (AtomPtr n, AtomPtr env) {
	return Atom::make_number(atom_eq (n->sequence.at(0), n->sequence.at(1)));
}
void replace (std::string &s, std::string from, std::string to) {
    int idx = 0;
    int next;
    while ((next = s.find (from, idx)) != std::string::npos) {
        s.replace (next, from.size (), to);
        idx = next + to.size ();
    } 
}
AtomPtr fn_string (AtomPtr node, AtomPtr env) {
	std::string cmd = node->sequence.at (0)->token;
	AtomPtr l = Atom::make_sequence();
	std::regex r;
	if (cmd == "length") {
		return Atom::make_number(type_check (node->sequence.at(1), AtomType::STRING, node)->token.size ());
	} else if (cmd == "find") {
		if (node->sequence.size () < 3) error ("invalid number of arguments in", node);
		unsigned long pos = type_check (node->sequence.at(1), AtomType::STRING, node)->token.find (
			type_check (node->sequence.at(2), AtomType::STRING, node)->token);
		if (pos == std::string::npos) return Atom::make_number (-1);
		else return Atom::make_number (pos);		
    } else if (cmd == "range") {
    	if (node->sequence.size () < 4) error ("invalid number of arguments in", node);
		std::string tmp = type_check (node->sequence.at(1), AtomType::STRING, node)->token.substr(
			type_check (node->sequence.at(2), AtomType::NUMBER, node)->value, 
			type_check (node->sequence.at(3), AtomType::NUMBER, node)->value);
		return Atom::make_string (tmp);		
	} else if (cmd == "replace") {
		if (node->sequence.size () < 4) error ("invalid number of arguments in", node);
		std::string tmp = type_check (node->sequence.at(1), AtomType::STRING, node)->token;
		replace (tmp,
			type_check (node->sequence.at(2), AtomType::STRING, node)->token, 
			type_check (node->sequence.at(3), AtomType::STRING, node)->token);
		return Atom::make_string(tmp);
	} else if (cmd == "regex") {
		if (node->sequence.size () < 3) error ("invalid number of arguments in", node);
		std::string str = type_check (node->sequence.at(1), AtomType::STRING, node)->token;
		std::regex r (type_check (node->sequence.at(2), AtomType::STRING, node)->token);
	    std::smatch m; 
	    std::regex_search(str, m, r);

	    AtomPtr l = Atom::make_sequence();
	    for(auto v: m) {
	    	l->sequence.push_back (Atom::make_string (v.str()));
	    }
		return l;		
	} else {
		error ("invalid string request", node->sequence.at (0));
	}
    return l;
}
AtomPtr fn_exec (AtomPtr node, AtomPtr env) {
	return Atom::make_number (system (
		type_check (node->sequence.at(0), AtomType::STRING, node)->token.c_str ()));
}

AtomPtr fn_exit (AtomPtr node, AtomPtr env) {
	int ret = 0;
	if (node->sequence.size ()) ret = (int) node->sequence.at (0)->value;
	exit (ret);
	return Atom::make_sequence ();
}
void add_builtin (const std::string& name, Builtin f, int minargs, AtomPtr env) {
	AtomPtr op = Atom::make_builtin (f, minargs); 
	op->token = name;
	extend (Atom::make_symbol (name), op, env);
}
#define DECLFFI(name, f) \
	AtomPtr name (AtomPtr node, AtomPtr env) { \
		AtomPtr out = nullptr; 	\
	 	f (node, env, out);\
	 	return out;\
	}

AtomPtr fn_import (AtomPtr params, AtomPtr env) {
		std::string name = getenv ("HOME");
#ifdef __APPLE__
		name += "/.quile/" + type_check (params->sequence.at(0), AtomType::STRING, params)->token + ".so";
#elif __linux
		name += "/.quile/" + type_check (params->sequence.at(0), AtomType::STRING, params)->token + ".so";
#else
		name += "/.quile/" + type_check (params->sequence.at(0), AtomType::STRING.params)->token + ".dll";
#endif
	void* handle = dlopen (name.c_str (), RTLD_NOW);
	if (!handle) {
		error (dlerror (), params);
	}
	unsigned ct = 0;
	for (unsigned i = 0; i < params->sequence.at(1)->sequence.size() / 2; ++i) {
		Builtin f = (Builtin) dlsym (handle, 
			type_check (params->sequence.at(1)->sequence.at(2 * i), AtomType::SYMBOL, params)->token.c_str ());
		
		if (f) {
			add_builtin(params->sequence.at(1)->sequence.at(2 * i)->token, 
				f, 
				type_check (params->sequence.at(1)->sequence.at(2 * i + 1), AtomType::NUMBER, params)->value, env); // silent error
			++ct;
		}
	}
	return ct == 0 ? Atom::make_sequence() : Atom::make_number(ct); // number of proc imported
}

// interface
AtomPtr make_env () {
	AtomPtr env = Atom::make_sequence ();
	env->sequence.push_back (Atom::make_sequence ()); // parent
    // environments
    add_builtin("set", fn_set<false>, 2, env);
    add_builtin("setrec", fn_set<true>, 2, env);
    add_builtin("updef", fn_updef, 2, env);
    add_builtin("unset", fn_unset, 0, env);
    add_builtin("\\", fn_lambda<false>, 2, env);
    add_builtin("@", fn_lambda<true>, 2, env);
    add_builtin("info", fn_info, 1, env);
	// sequences
	add_builtin ("list", fn_list, 0, env);
	add_builtin ("lindex", fn_lindex, 1, env);
	add_builtin ("lrange", fn_lrange, 2, env);
	add_builtin ("llength", fn_llength, 1, env);
	add_builtin ("ljoin", fn_ljoin, 1, env);
    // flow control
    add_builtin("eval", fn_eval, 1, env);
    add_builtin("if", fn_if, 2, env);
    add_builtin("while", fn_while, 2, env);
    add_builtin("continue", fn_passby<0>, 0, env);
    add_builtin("break", fn_passby<1>, 0, env);
    add_builtin("pass", fn_passby<2>, 1, env);
    add_builtin ("catch", fn_catch, 3, env);
    add_builtin ("throw", fn_throw, 1, env);	
    //I/O
    add_builtin("puts", fn_format<0>, 1, env);
    add_builtin("tostr", fn_format<1>, 1, env);    
    add_builtin("save", fn_format<2>, 1, env);
    add_builtin("gets", fn_gets, 0, env);
    add_builtin("source", fn_source, 1, env);
    extend(Atom::make_symbol("nl"), Atom::make_symbol("\n"), env);
    // arithmetics/logic
	add_builtin ("+", fn_add, 1, env);
	add_builtin ("-", fn_sub, 1, env);
	add_builtin ("*", fn_mul, 1, env);
	add_builtin ("/", fn_div, 1, env);
	add_builtin ("<", fn_less, 1, env);
	add_builtin (">", fn_gt, 1, env);
	add_builtin ("<=", fn_lesseq, 1, env);
	add_builtin (">=", fn_gteq, 1, env);	
	add_builtin ("sin", fn_sin, 1, env);
	add_builtin ("cos", fn_cos, 1, env);
	add_builtin ("log", fn_log, 1, env);
	add_builtin ("exp", fn_exp, 1, env);
	add_builtin ("abs", fn_fabs, 1, env);
	add_builtin ("sqrt", fn_sqrt, 1, env);
	add_builtin ("floor", fn_floor, 1, env);    
    add_builtin("eq", fn_eq, 2, env);
    //others
	add_builtin ("string", fn_string, 2, env);
	add_builtin ("exec", fn_exec, 1, env);
	add_builtin ("exit", fn_exit, 0, env);
	add_builtin ("import", fn_import, 0, env);    
    return env;
}
void repl (AtomPtr env, std::istream& in, std::ostream& out) {
	#if defined (ENABLE_READLINE)
	char* line_read = 0;
	#endif
	std::istream* current = &in;
	while (true){
		#if defined (ENABLE_READLINE)
			if (line_read) {
				delete [] line_read;
				line_read = 0;
			}
			line_read = readline ("> ");
			if (!line_read) break;
			if (line_read && *line_read) add_history (line_read);
			std::string input (line_read);
			if (!input.size ()) { continue; }
			std::stringstream tmp_str; 
			tmp_str << line_read << "\n"; // lf added
			current = &tmp_str;
		#else
			out << "> ";
		#endif	
		try {
			puts (eval (gets (*current), env), out);
			out << std::endl;
		} catch (std::exception& e) {
			out << RED << "error: " << e.what () << RESET << std::endl;
		} catch (AtomPtr& e) {
			out << RED << "error: uncaught expection " << RESET;
			puts (e, out); out << std::endl;
		} catch (...) {
			out << RED << "fatal error: execution stopped" << RESET << std::endl;
		}
	} 
}

// EOF

#endif // QUILE_H

