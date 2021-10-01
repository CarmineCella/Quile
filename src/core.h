// core.h
//
#ifndef CORE_H
#define CORE_H

#include <deque>
#include <string>
#include <sstream>
#include <stdexcept>
#include <fstream>
#include <memory>
#include <iostream>
#include <map>
#include <regex>
#include <valarray>
#include <cmath>
#include <dlfcn.h>
#if defined (ENABLE_READLINE)
	#include <readline/readline.h>
	#include <readline/history.h>
#endif

#define BOLDBLUE    "\033[1m\033[34m"
#define RED     	"\033[31m" 
#define RESET   	"\033[0m"

// TODO: line numbers, improve error messages, array tests, system, plotting, better examples, pvoc, grain

// ast
struct Atom;
typedef std::shared_ptr<Atom> AtomPtr;
typedef double Real;
typedef AtomPtr (*Builtin) (AtomPtr, AtomPtr);
enum AtomType {ARRAY, SYMBOL, STRING, LIST, STREAM, PROC, BUILTIN, OBJECT};
const char* TYPE_NAMES[] = {"array", "symbol", "string", "list", "stream", "proc", "builtin", "object"};
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
	std::deque<AtomPtr> sequence;
	std::valarray<Real> array;
	Builtin func;
	int minargs;
	void* obj;
	static AtomPtr make_sequence (bool is_stream = false) { 
		AtomPtr l = std::make_shared<Atom> (_constructor_tag{}); 
		l->type = is_stream ? AtomType::STREAM : AtomType::LIST;
		return l; 
	}
	static AtomPtr make_array (Real in) { 	
		std::valarray<Real> v (1);
		v[0] = in;
		return Atom::make_array (v);
	}
	static AtomPtr make_array (std::valarray<Real>& in) { 
		AtomPtr l = std::make_shared<Atom> (_constructor_tag{}); 
		l->type = AtomType::ARRAY;
		l->array = in;
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
	    case AtomType::ARRAY: return (x->array == y->array).min ();
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
	std::stringstream tmp2;
	tmp << err;
	if (node) {
		puts (node, tmp2, true);
		if (tmp2.str ().size () > 75) {
			std::string cut = tmp2.str ();
			cut = cut.substr (0, 75);
			tmp2.str ("");
			tmp2 << cut << "...";
		}
	}
	tmp << " " << tmp2.str ();
	throw std::runtime_error (tmp.str ());
}
AtomPtr args_check (AtomPtr node, unsigned ct, AtomPtr ctx = Atom::make_sequence ()) {
	if (node->sequence.size () < ct) {
		error ("insufficient number of arguments in", is_null(ctx) ? node : ctx);
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
			code->sequence.push_back(Atom::make_array(atof (token.c_str ())));
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
	AtomPtr outer = Atom::make_sequence();
	AtomPtr inner = Atom::make_sequence(true); // always executable
	for (unsigned t = 0; t < type_check (ct, AtomType::LIST, ct)->sequence.size ();  ++t) {
		if (ct->sequence.at(t)->token == "\n") {
			if (!is_null (inner)) outer->sequence.push_back(inner);
			inner = Atom::make_sequence(true);
		} else inner->sequence.push_back(ct->sequence.at(t));
	}
	if (!is_null (inner)) outer->sequence.push_back(inner);
	if (is_null (outer)) error ("invalid block", outer);
	return outer;
}
AtomPtr fn_eval (AtomPtr b,  AtomPtr env) { return nullptr; } // dummy
AtomPtr fn_apply (AtomPtr b,  AtomPtr env) { return nullptr; } // dummy
AtomPtr fn_if (AtomPtr b,  AtomPtr env) { return nullptr; } // dummy
AtomPtr fn_break (AtomPtr b,  AtomPtr env) { return Atom::make_sequence (); }
AtomPtr fn_continue (AtomPtr b,  AtomPtr env) { return Atom::make_sequence (); }
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
		if (cmd->func == &fn_break || cmd->func == &fn_continue) {
			throw cmd;
		}
    	if (cmd->func == &fn_if) {
    		bool has_else = false;
			if (params->sequence.size () >  2) {
				if  (params->sequence.at (2)->token != "else" || params->sequence.size () != 4) {
					error ("invalid if/else syntax in ", node);
				}    		
				has_else = true;
			}
    		AtomPtr code = split_sequence (params->sequence.at (1));
			AtomPtr cond =  eval (type_check (params->sequence.at (0), AtomType::LIST, params), env);
    		if (type_check (cond, AtomType::ARRAY, params)->array[0]) {
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
		} else if (cmd->func == &fn_apply) {
			params->sequence.at (1)->sequence.push_front (params->sequence.at(0));
			node = params->sequence.at(1);
			goto tail_call;					
		}
    	return cmd->func (params, env);
    } else error ("function expected in", cmd);
    return Atom::make_sequence();
}
// builtins
std::ostream& puts (AtomPtr node, std::ostream& out, bool is_write = false) {
	switch (node->type) {
		case ARRAY:
			for (unsigned i = 0; i < node->array.size (); ++i) {
				out << node->array[i] << (i == node->array.size () - 1 ? "" : " ");
			}
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
			return Atom::make_array(0);
		}	
	}
	return Atom::make_array(1);
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
			l->sequence.push_back(Atom::make_array(ans));
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
AtomPtr fn_lcar (AtomPtr params, AtomPtr env) {
	AtomPtr l = type_check (params->sequence.at (0), AtomType::LIST, params);
	if (is_null(l)) return Atom::make_sequence();
	return l->sequence.at(0);
}
AtomPtr fn_lrange (AtomPtr params, AtomPtr env) {
	AtomPtr l = type_check (params->sequence.at (0), AtomType::LIST, params);
	int i = (int) (type_check(params->sequence.at (1), AtomType::ARRAY, params)->array[0]);
	int len = (int) (type_check(params->sequence.at (2), AtomType::ARRAY, params)->array[0]);
	int stride = 1;
	if (params->sequence.size () == 4) {
		stride  = (int) (type_check(params->sequence.at (3), AtomType::ARRAY, params)->array[0]);
	}
	if (i < 0 || len < 0 || i + len  > l->sequence.size ()) {
		return Atom::make_sequence();
	}
	AtomPtr nl = Atom::make_sequence();
	for (int j = i; j < i + len; j += stride) nl->sequence.push_back(l->sequence.at (j));
	return nl;
}
AtomPtr fn_lreplace (AtomPtr params, AtomPtr env) {
	AtomPtr l = type_check (params->sequence.at (0), AtomType::LIST, params);
	AtomPtr r = type_check (params->sequence.at (1), AtomType::LIST, params);
	int i = (int) (type_check(params->sequence.at (2), AtomType::ARRAY, params)->array[0]);
	int len = (int) (type_check(params->sequence.at (3), AtomType::ARRAY, params)->array[0]);
	int stride = 1;
	if (params->sequence.size () == 5) {
		stride  = (int) (type_check(params->sequence.at (4), AtomType::ARRAY, params)->array[0]);
	}
	if (i < 0 || len < 0 || stride < 1 || i + len  > l->sequence.size () || (int) (len / stride) > r->sequence.size ()) {
		return Atom::make_sequence();
	}
	AtomPtr nl = Atom::make_sequence();
	int p = 0;
	for (int j = i; j < i + len; j += stride) {
		l->sequence.at(j) = r->sequence.at (p);
		++p;
	}
	return r;
}
AtomPtr fn_llength (AtomPtr params, AtomPtr env) {
	AtomPtr l = type_check (params->sequence.at (0), AtomType::LIST, params);
	return Atom::make_array (l->sequence.size ());
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
	AtomPtr cond = type_check (b->sequence.at (0), AtomType::LIST, b);
	AtomPtr code = split_sequence (b->sequence.at (1));
	while (type_check (eval (cond, env), AtomType::ARRAY, b)->array[0]) {
		for (unsigned i = 0; i < code->sequence.size (); ++i) {
			AtomPtr p = code->sequence.at (i);
			try {
				res = eval (p, env);
			} catch (AtomPtr& e) {
				if (atom_eq (e, Atom::make_builtin (&fn_break))) return res;
				if (atom_eq (e, Atom::make_builtin (&fn_continue))) ++i;
				return res;
			}
		}
	}
	return res;
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
			std::cout << tmp.str (); std::cout.flush ();
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
			return Atom::make_array(1);
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
AtomPtr fn_eq (AtomPtr n, AtomPtr env) {
	return Atom::make_array(atom_eq (n->sequence.at(0), n->sequence.at(1)));
}
void array_from_list (AtomPtr list, std::valarray<Real>& out) {
	int total = 0;
	for (unsigned i = 0; i < list->sequence.size (); ++i) {
		total +=  type_check (list->sequence.at (i), AtomType::ARRAY, list)->array.size ();
	}
	out.resize (total);
	int p = 0;
	for (unsigned i = 0; i < list->sequence.size (); ++i) {
		AtomPtr v = type_check (list->sequence.at (i), AtomType::ARRAY, list);
		for (unsigned j = 0; j < v->array.size (); ++j) {
			out[p] = v->array[j];
			++p;
		}
	}
}
AtomPtr fn_array (AtomPtr n, AtomPtr env) {
	std::valarray<Real> v;
	array_from_list (n, v);
	return Atom::make_array(v);
}
#define MAKE_ARRAYBINOP(op,name)									\
	AtomPtr name (AtomPtr n, AtomPtr env) {						\
		AtomPtr v1 = type_check  (n->sequence.at (0), AtomType::ARRAY, n); \
		AtomPtr v2 = type_check  (n->sequence.at (1), AtomType::ARRAY, n); \
		if (v1->array.size () != v2->array.size ()) error ("array must have the same size in", n);\
		std::valarray<Real> v (v1->array op v2->array); \
		return  Atom::make_array (v); \
	}\

MAKE_ARRAYBINOP (+, fn_add);
MAKE_ARRAYBINOP (-, fn_sub);
MAKE_ARRAYBINOP (*, fn_mul);
MAKE_ARRAYBINOP (/, fn_div);

#define MAKE_ARRAYCMPOP(op,name)									\
	AtomPtr name (AtomPtr n, AtomPtr env) {						\
		AtomPtr v1 = type_check  (n->sequence.at (0), AtomType::ARRAY, n); \
		AtomPtr v2 = type_check  (n->sequence.at (1), AtomType::ARRAY, n); \
		if (v1->array.size () != v2->array.size ()) error ("array must have the same size in", n);\
		std::valarray<bool> b (v1->array op v2->array); \
		std::valarray<Real> v (b.size ()); \
		for (unsigned i = 0; i < v.size (); ++i) v[i] = (Real) b[i];\
		return  Atom::make_array (v); \
	}\

MAKE_ARRAYCMPOP (==, fn_same);
MAKE_ARRAYCMPOP (!=, fn_different);
MAKE_ARRAYCMPOP (<, fn_less);
MAKE_ARRAYCMPOP (<=, fn_lesseq);
MAKE_ARRAYCMPOP (>, fn_gt);
MAKE_ARRAYCMPOP (>=, fn_gteq);

#define MAKE_ARRAYSINGOP(op,name)									\
	AtomPtr name (AtomPtr n, AtomPtr env) {						\
		std::valarray<Real> v = op (type_check(n->sequence.at (0), AtomType::ARRAY, n)->array); \
		return  Atom::make_array (v); \
	}\

MAKE_ARRAYSINGOP (std::abs, fn_abs);
MAKE_ARRAYSINGOP (exp, fn_exp);
MAKE_ARRAYSINGOP (log, fn_log);
MAKE_ARRAYSINGOP (log10, fn_log10);
MAKE_ARRAYSINGOP (sqrt, fn_sqrt);
MAKE_ARRAYSINGOP (sin, fn_sin);
MAKE_ARRAYSINGOP (cos, fn_cos);
MAKE_ARRAYSINGOP (tan, fn_tan);

#define MAKE_ARRAYMETHODS(op,name)									\
	AtomPtr name (AtomPtr n, AtomPtr env) {						\
		return  Atom::make_array (type_check (n->sequence.at (0), AtomType::ARRAY, n)->array.op ()); \
	}\

MAKE_ARRAYMETHODS (min, fn_min);
MAKE_ARRAYMETHODS (max, fn_max);
MAKE_ARRAYMETHODS (sum, fn_sum);
MAKE_ARRAYMETHODS (size, fn_size);
AtomPtr fn_mean (AtomPtr node, AtomPtr env) {
	AtomPtr v1 = type_check  (node->sequence.at (0), AtomType::ARRAY, node);
	return Atom::make_array (v1->array.sum () / v1->array.size ());
}
AtomPtr fn_slice (AtomPtr node, AtomPtr env) {
	AtomPtr v1 = type_check  (node->sequence.at (0), AtomType::ARRAY, node);
	int i = (int) type_check  (node->sequence.at (1), AtomType::ARRAY, node)->array[0];
	int len = (int) type_check  (node->sequence.at (2), AtomType::ARRAY, node)->array[0];
	int stride = 1;
	if (node->sequence.size () == 4) stride = (int) type_check  (node->sequence.at (3), AtomType::ARRAY, node)->array[0];
	
	if (i < 0 || len < 1 || stride < 1 || i + len  > v1->array.size ()) {
		error ("invalid indexing for slice", node);
	}
	std::valarray<Real> s = v1->array[std::slice (i, len, stride)];
	return Atom::make_array (s);
}
AtomPtr fn_assign (AtomPtr node, AtomPtr env) {
	AtomPtr v1 = type_check  (node->sequence.at (0), AtomType::ARRAY, node);
	AtomPtr v2 = type_check  (node->sequence.at (1), AtomType::ARRAY, node);
	int i = (int) type_check  (node->sequence.at (2), AtomType::ARRAY, node)->array[0];
	int len = (int) type_check  (node->sequence.at (3), AtomType::ARRAY, node)->array[0];
	int stride = 1;
	if (node->sequence.size () == 5) stride = (int) type_check  (node->sequence.at (4), AtomType::ARRAY, node)->array[0];
	if (i < 0 || len < 1 || stride < 1 || i + len  > v1->array.size () || (int) (len / stride) > v2->array.size ()) {
		error ("invalid indexing for assign", node);
	}
	v1->array[std::slice(i, len, stride)] = v2->array;
	return v1;
}
AtomPtr fn_pow (AtomPtr n, AtomPtr env) {
	AtomPtr v1 = type_check  (n->sequence.at (0), AtomType::ARRAY, n); 
	AtomPtr v2 = type_check  (n->sequence.at (1), AtomType::ARRAY, n); 
	if (v1->array.size () != v2->array.size ()) error ("array must have the same size in", n);
	std::valarray<Real> v = std::pow (v1->array,  v2->array);
	return Atom::make_array (v);
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
		return Atom::make_array(type_check (node->sequence.at(1), AtomType::STRING, node)->token.size ());
	} else if (cmd == "find") {
		if (node->sequence.size () < 3) error ("invalid number of arguments in", node);
		unsigned long pos = type_check (node->sequence.at(1), AtomType::STRING, node)->token.find (
			type_check (node->sequence.at(2), AtomType::STRING, node)->token);
		if (pos == std::string::npos) return Atom::make_array (-1);
		else return Atom::make_array (pos);		
    } else if (cmd == "range") {
    	if (node->sequence.size () < 4) error ("invalid number of arguments in", node);
		std::string tmp = type_check (node->sequence.at(1), AtomType::STRING, node)->token.substr(
			type_check (node->sequence.at(2), AtomType::ARRAY, node)->array[0], 
			type_check (node->sequence.at(3), AtomType::ARRAY, node)->array[0]);
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
	return Atom::make_array (system (
		type_check (node->sequence.at(0), AtomType::STRING, node)->token.c_str ()));
}
AtomPtr fn_exit (AtomPtr node, AtomPtr env) {
	int ret = 0;
	if (node->sequence.size ()) ret = (int) node->sequence.at (0)->array[0];
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
				type_check (params->sequence.at(1)->sequence.at(2 * i + 1), AtomType::ARRAY, params)->array[0], env); // silent error
			++ct;
		}
	}
	return ct == 0 ? Atom::make_sequence() : Atom::make_array(ct); // number of proc imported
}
// interface
AtomPtr make_env () {
	AtomPtr env = Atom::make_sequence ();
	env->sequence.push_back (Atom::make_sequence ()); // parent
    // environments
    add_builtin("set", fn_set<false>, 2, env);
    // add_builtin("setrec", fn_set<true>, 2, env);
    add_builtin("updef", fn_updef, 2, env);
    add_builtin("unset", fn_unset, 1, env);
    add_builtin("\\", fn_lambda<false>, 2, env);
    add_builtin("@", fn_lambda<true>, 2, env);
    add_builtin("info", fn_info, 1, env);
	add_builtin("eval", fn_eval, 1, env);
	add_builtin("->", fn_apply, 2, env);
	// sequences
	add_builtin ("list", fn_list, 0, env);
	add_builtin ("car", fn_lcar, 1, env);
	add_builtin ("lrange", fn_lrange, 3, env);
	add_builtin ("lreplace", fn_lreplace, 4, env);
	add_builtin ("llength", fn_llength, 1, env);
	add_builtin ("ljoin", fn_ljoin, 1, env);
    // flow control
    add_builtin("if", fn_if, 2, env);
    add_builtin("while", fn_while, 2, env);
    add_builtin("continue", fn_continue, 0, env);
    add_builtin("break", fn_break, 0, env);
    add_builtin ("catch", fn_catch, 3, env);
    add_builtin ("throw", fn_throw, 1, env);	
    //I/O
    add_builtin("puts", fn_format<0>, 1, env);
    add_builtin("tostr", fn_format<1>, 1, env);    
    add_builtin("save", fn_format<2>, 1, env);
    add_builtin("gets", fn_gets, 0, env);
    add_builtin("source", fn_source, 1, env);
    extend(Atom::make_symbol("nl"), Atom::make_symbol("\n"), env);
    // arithmetics/arrays
    add_builtin("eq", fn_eq, 2, env);
	add_builtin ("array", fn_array, 1, env);
	add_builtin ("+", fn_add, 2, env);
	add_builtin ("-", fn_sub, 2, env);
	add_builtin ("*", fn_mul, 2, env);
	add_builtin ("/", fn_div, 2, env);
	add_builtin ("==", fn_same, 2, env);
	add_builtin ("!=", fn_different, 2, env);
	add_builtin ("<", fn_less, 2, env);
	add_builtin (">", fn_gt, 2, env);
	add_builtin ("<=", fn_lesseq, 2, env);
	add_builtin (">=", fn_gteq, 2, env);	
	add_builtin ("pow", fn_pow, 2, env);
	add_builtin ("log", fn_log, 1, env);
	add_builtin ("log10", fn_log10, 1, env);
	add_builtin ("exp", fn_exp, 1, env);
	add_builtin ("abs", fn_abs, 1, env);
	add_builtin ("sqrt", fn_sqrt, 1, env);
	add_builtin ("sin", fn_sin, 1, env);
	add_builtin ("cos", fn_cos, 1, env);
	add_builtin ("tan", fn_tan, 1, env);
	add_builtin ("min", fn_min, 1, env);
	add_builtin ("max", fn_max, 1, env);
	add_builtin ("sum", fn_sum, 1, env);
	add_builtin ("mean", fn_mean, 1, env);
	add_builtin ("size", fn_size, 1, env);
	add_builtin ("slice", fn_slice, 3, env);
	add_builtin ("assign", fn_assign, 4, env);
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

#endif // CORE_H
