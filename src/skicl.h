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

struct Node;
typedef std::shared_ptr<Node> Node_ptr;
typedef Node_ptr (*Functor)(Node_ptr,Node_ptr);

enum NodeType {
	LEXEME,
	QUOTE,
	BLOCK
};

struct Node {
	Node (NodeType t) {
		type = t;
		action = nullptr;
	}
	NodeType type;
	std::string lexeme;
	std::deque <Node_ptr> tail;
	Functor action;
	static Node_ptr create (NodeType t) {
		return std::make_shared<Node> (t);
	}
};

std::string get_token (std::istream& in) {
	std::stringstream accum;
	while (!in.eof()) {
		char c = in.get ();
		switch(c) {
			case '$':
				if (accum.str ().size ()) {
					std::string tmp = accum.str ();
					accum.str ().clear ();
					in.putback (c);
					return tmp;
				} else {
					accum << c;
				}
			break;
			case '\n': case '[': case ']': case '{': case '}':
				if (accum.str ().size ()) {
					in.putback (c);
				} else {
					accum << c;
				}
				return accum.str ();
			break;
			case '\t': case ' ':  case '\r':
				if (accum.str ().size ()) {
					return accum.str ();
				} else continue;
			break;
			case '\"':
				accum << c;
				while (!in.eof ()) {
					c = in.get ();
					accum << c;
					if (c == '\"') break;
				}
				return accum.str ().substr (1, accum.str ().size () - 2);
			break;
			default:
				if (!in.eof ()) accum << c;
			break;
		}
	}
	return accum.str ();
}

void error (const std::string& err, Node_ptr node);
Node_ptr read (std::istream& in) {
	std::string token = get_token (in);

	if (token == "[" || token == "{") {
		Node_ptr l = Node::create (token == "[" ? BLOCK : QUOTE);
		std::string term = (token == "[" ? "]" : "}");
		std::string nterm = (token == "[" ? "}" : "]");
		while (!in.eof ()) {
			Node_ptr n = read (in);
			if (n->lexeme == "\n") continue;
			if (n->lexeme == term) break;
			if (n->lexeme == nterm) {
				error ("invalid terminator in expression", 
					Node::create (QUOTE));
			}
			l->tail.push_back(n);
		}
		return l;
	} else {
		Node_ptr s = Node::create (LEXEME);
		s->lexeme = token;
		return s;
	}
}
Node_ptr read_block (std::istream& in) {
	Node_ptr bl = Node::create(BLOCK);
	while (!in.eof ()) {
		Node_ptr n = read (in);
		if (n->lexeme == "\n") break;
		bl->tail.push_back(n);
	}
	return bl;
}
std::ostream& print (Node_ptr n, std::ostream& out) {
	switch (n->type) {
		case LEXEME:
			out << n->lexeme;
		break;
		case QUOTE:	case BLOCK:
		if (n->type == QUOTE) out << "{";
		else out << "[";
		for (unsigned i = 0; i < n->tail.size (); ++i) {
			print (n->tail.at (i), out); 
			if (i != n->tail.size () - 1) out << " ";
		}
		if (n->type == QUOTE) out << "}";
		else out << "]";		
		break;
	}
	return out;
}
void error (const std::string& err, Node_ptr node) {
	std::stringstream tmp;
	tmp << err;
	if (node) {
		std::stringstream tmp2;
		print (node, tmp2);
		tmp << " " << tmp2.str ();
	}
	throw std::runtime_error (tmp.str ());
}
bool is_empty (Node_ptr node) { 
	return (!node || (node->type == QUOTE && node->tail.size () == 0)); 
}
bool atom_eq (Node_ptr a, Node_ptr b) {
	if (a->type != b->type) return false;
	switch (a->type) {
		case LEXEME:
			if (a->lexeme != b->lexeme) return false;
		break;
		case QUOTE: case BLOCK:
			if (a->tail.size () != b->tail.size ()) return false;
			for (unsigned i = 0; i < a->tail.size (); ++i) {
				if (!atom_eq (a->tail.at (i), b->tail.at (i))) return false;
			}
		break;
	}
	return true;
}
Node_ptr assoc (Node_ptr key, Node_ptr env) {
tail_call:
	for (unsigned i = 1; i <= env->tail.size () / 2; ++i) {
		if (atom_eq (env->tail.at (2 * i - 1), key)) return env->tail.at (2 * i);
	}
	Node_ptr outer = env->tail.at (0);
	if (!is_empty (outer)) { env = outer; goto tail_call; }
	error ("unbound symbol", key);
	return Node::create (QUOTE); // not reached
}
Node_ptr extend (Node_ptr key, Node_ptr val, Node_ptr env, bool recurse = false) {
	for (unsigned i = 1; i <= env->tail.size () / 2; ++i) {
		if (atom_eq (env->tail.at (2 * i - 1), key)) {
			env->tail.at (2 * i) = val; 
			return val;
		}
	}
	if (recurse) {
		if (!is_empty (env->tail.at (0))) {
			return extend(key, val, env->tail.at (0), recurse);
		}
	} 
	env->tail.push_back (key); env->tail.push_back (val);
	return val;
}
Node_ptr eval (Node_ptr node, Node_ptr env) {
	print (node, std::cout) << std::endl;
	if (node->type == QUOTE) return node;
	if (node->type == LEXEME) {
		return assoc (node, env);
	}
	// BLOCK here
	if (node->tail.size () == 0) return node;
	Node_ptr args = Node::create(QUOTE);
	for (unsigned i = 0; i < node->tail.size (); ++i) {
		args->tail.push_back (eval (node->tail.at (i), env));
	}
	Node_ptr cmd = args->tail.at (0);
	args->tail.pop_front ();
	if (cmd->action != nullptr) {
		return cmd->action (args, env);
	}

	error ("invalid function in", node);
	return node;
}
Node_ptr set (Node_ptr n, Node_ptr env) {
	if (n->tail.size () < 1) error ("insufficient number of args in ", n);
	if (n->tail.size () == 1) {
		return assoc (n->tail.at (0), env);
	} else {
		return extend (n->tail.at(0)->tail.at (0), 
			n->tail.at (1)->tail.at (0), env);		
	}
}
Node_ptr init_env () {
	Node_ptr env = Node::create(QUOTE);
	env->tail.push_back (Node::create(QUOTE));
	Node_ptr s = Node::create (LEXEME);
	s->lexeme = "set";
	Node_ptr f = Node::create(LEXEME);
	f->lexeme = "set";
	f->action = &set;
	extend(s, f, env);
	return env;
}

void repl (Node_ptr env) {
	std::cout << env->tail.size () << std::endl;
	std::cout << ">> ";
	while (true) {
		try {
			print (eval (read_block (std::cin), env), std::cout);
			std::cout << std::endl;
		} catch (std::exception& e) {
			std::cout << "error: " << e.what () << std::endl;
		}
	}
}
#endif // SKICL_H