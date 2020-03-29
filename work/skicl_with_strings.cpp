// micol.cpp
// 
// TODO: primitive, controllo err (args, tipo), if, while, upval, eval, serializzazione, test
// speed, tail ricorsione, lazy eval, static scoping, numlinea, return, break, continue
#include <iostream>
#include <fstream>
#include <deque>
#include <sstream>
#include <map>
#include <stdexcept>
#include <variant>

using namespace std;

// ast
typedef double Real;
typedef std::deque<std::string> Block;
struct Namespace;
typedef std::string (*Action) (Block& b, Namespace&);
struct Namespace {
    std::map<std::string, std::variant<std::string, Block> > env;
    std::map<std::string, Action> func;
};

// lexing
std::string get_token (std::istream& input) {
	std::stringstream accum;
	while (!input.eof ()) {
		char c = input.get ();
		switch (c) {
			case '[': case ']': case '\n':  case '{': case '}':
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
				return accum.str ().substr (1, accum.str ().size () - 2);
			break;            
			default:
				if (c > 0) accum << c;
		}
	}
	return accum.str ();
}
// bool is_number (const char* s) {
// 	std::istringstream iss (s);
// 	Real dummy;
// 	iss >> std::noskipws >> dummy;
// 	return iss && iss.eof ();
// }
// parsing and evaluation
std::string eval (std::istream& in, Namespace& nspace) {
    Block b;
    while (!in.eof ()) {
        std::string token = get_token (in);
        if (token.size () == 0) continue;
        if (token == "\n") break;
        if (token[0] == '$') {
            std::string key = token.substr (1, token.size () - 1);
            if (nspace.env.find (key) != nspace.env.end ()) {
                b.push_back (std::get<std::string> (nspace.env[key]));
            } else {
                std::stringstream err;
                err << "undeclared identifier " << key;
                throw std::runtime_error (err.str ());
            }
        } 
        
        else if (token == "{") {
            std::stringstream code;
             while (!in.eof ()) {
                token = get_token(in);
                if (token == "}") break;
                code << token << " ";
            }
            b.push_back (code.str ());
        } 
        
        else if (token == "[") {
            std::stringstream code;
            while (!in.eof ()) {
                token = get_token(in);
                if (token == "]") break;
                code << token << " ";
            }
            b.push_back (eval (code, nspace));
        } else b.push_back (token);
    }
    if (b.size () == 0) return "";
    std::string cmd = b.at (0);
    b.pop_front ();
    if (nspace.env.find (cmd) != nspace.env.end ()) {
        std::stringstream argstream (std::get<Block>(nspace.env[cmd]).at (0));
        std::stringstream code (std::get<Block>(nspace.env[cmd]).at (1));
        Block args;
        while (!argstream.eof ()) {
            args.push_back (get_token (argstream));
        }
        Namespace nnspace = nspace;
        for (unsigned i = 0; i < b.size (); ++i) {
            nnspace.env[args.at (i)] = b.at (i);
        }
        std::string res;
        while (!code.eof ()) {
            res = eval (code, nnspace);
        }
        return res;
    } else if (nspace.func.find (cmd) != nspace.func.end ()) {
        return nspace.func[cmd](b, nspace);
    } else {
        std::stringstream err;
        err << "undeclared command " << cmd;
        throw std::runtime_error (err.str());
    }
    return "";
}

// functors
std::string fn_puts (Block& b, Namespace& nspace) {
    std::cout << b.at (0) << std::endl;
    return "";
}
std::string fn_set (Block& b,  Namespace& nspace) {
    std::string key = b.at (0);
    nspace.env[key] = b.at (1);
    return b.at (1);
}
std::string fn_proc (Block& b, Namespace& nspace) {
   std::string key = b.at (0);
    b.pop_front ();
    nspace.env[key] = b;
    return key;
}
template <char op>
std::string fn_binop (Block& b,  Namespace& nspace) {
    std::stringstream val;
    Real first = atof (b.at (0).c_str ()); 
    Real second = atof (b.at (1).c_str ());;
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
    return val.str ();
}
std::string load (const std::string& name, Namespace& nspace) {
	std::ifstream in (name.c_str ());
	if (!in.good ()) {
		std::string longname = getenv("HOME");
		longname += "/.skicl/" + name;
		in.open (longname.c_str());
		if (!in.good ()) return "";
	}
    std::string res;
	while (!in.eof ()) {
		res = eval (in, nspace);
	}
	return res;
}
std::string fn_load (Block& b,  Namespace& nspace) {
    return load (b.at (0), nspace);
}

// interface
void init_namespace (Namespace& nspace) {
    nspace.func["puts"] = fn_puts;
    nspace.func["load"] = fn_load;
    nspace.func["set"] = fn_set;
    nspace.func["proc"] = fn_proc;
    nspace.func["add"] = fn_binop<'+'>;
    nspace.func["sub"] = fn_binop<'-'>;
    nspace.func["mul"] = fn_binop<'*'>;
    nspace.func["div"] = fn_binop<'/'>;
    nspace.func["le"] = fn_binop<'<'>;
    nspace.func["gr"] = fn_binop<'>'>;
    nspace.func["leq"] = fn_binop<'L'>;
    nspace.func["greq"] = fn_binop<'G'>;
    nspace.func["eq"] = fn_binop<'='>;
}
void repl (std::istream& in, Namespace& nspace){
    while (true) { 
        cout << ">> ";
        try {
            std::cout << eval (in, nspace) << std::endl;
        } catch (std::exception& err) {
            std::cout << "error: " << err.what () << std::endl;
        }
    } 
}
int main (int argc, char* argv[]) {
	try {
	    Namespace nspace;
        init_namespace (nspace);

		if (argc > 1) {
			for (unsigned i = 1; i < argc; ++i) {
				std::string res = load (argv[i], nspace);
			}
		} else {
			cout << "[skicl, ver 0.1]" << endl << endl;
			cout << "a tiny scheme+tcl dialect" << endl;
			cout << "(c) 2020, www.carminecella.com" << endl << endl;
            repl (cin, nspace);
		}
	} catch (exception& e) {
		cout << "error: " << e.what () << endl;
	} catch (...) {
		cout << "fatal error; quitting" << endl;
	}

    return 0;
}