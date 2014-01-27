#!/usr/bin/env python

import re
import sys

class ParseError(Exception):
    def __init__(self, message, line, column):
        self.message = message
        self.line = line
        self.column = column

    def __str__(self):
        return "error at %d:%d: %s" % (self.line, self.column, self.message)


class Lexer(object):
    def __init__(self, text):
        self.text = text
        self.len = len(self.text)
        self.pos = 0
        self.line = 1
        self.column = 1

    def eof(self):
        return self.pos >= self.len

    def skip_whitespace_and_comments(self):
        in_comment = False
        while self.pos < self.len:
            if self.text[self.pos:].startswith("\r\n"):
                self.line += 1
                self.column = 1
                self.pos += 2
                in_comment = False
            elif self.text[self.pos] == "\n":
                self.line += 1
                self.column = 1
                self.pos += 1
                in_comment = False
            elif self.text[self.pos] == " ":
                self.column += 1
                self.pos += 1
            elif self.column == 1 and self.text[self.pos] == "#":
                in_comment = True
                self.pos += 1
                self.column += 1
            else:
                if in_comment:
                    self.column += 1
                    self.pos += 1
                else:
                    break

    def consume(self, pattern, kind=None):
        m = re.match("^(%s)" % pattern, self.text[self.pos:])
        if m:
            group = m.group(1)
            if kind is None:
                kind = pattern
            result = (group, kind, self.line, self.column)
            self.pos += len(group)
            self.column += len(group)
            #print "consume(%s, %s) = %s" % (pattern, kind, repr(result))
            return result
        else:
            #print "consume(%s, %s) = None" % (pattern, kind)
            return None

    def next_token(self):
        self.saved_pos = self.pos
        self.saved_line = self.line
        self.saved_column = self.column
        self.skip_whitespace_and_comments()
        if self.pos >= self.len:
            return (None, "eof", self.line, self.column)
        else:
            return self.consume("left") \
                or self.consume("right") \
                or self.consume("forward") \
                or self.consume("backward") \
                or self.consume("fire") \
                or self.consume("vid:pid") \
                or self.consume("report_id") \
                or self.consume("\\[", "[") \
                or self.consume("\\]", "]") \
                or self.consume("!=") \
                or self.consume("=") \
                or self.consume("<") \
                or self.consume(">") \
                or self.consume("&") \
                or self.consume(":") \
                or self.consume("[0-9a-fA-F]{4}", "word") \
                or self.consume("[0-9a-fA-F]{1,2}", "byte") \
                or self.consume("[^ \\r\\n]+")

    def go_back(self):
        self.pos = self.saved_pos
        self.line = self.saved_line
        self.column = self.saved_column


class Parser(object):
    def __init__(self, lexer):
        self.lexer = lexer
        self.data = []

    def expect(self, *args):
        #print "\nexpect", args
        token, kind, line, column = self.lexer.next_token()
        if not kind in args:
            if len(args) > 1:
                raise ParseError("expected one of: %s; got %s" % (", ".join(args), token), line, column)
            else:
                raise ParseError("expected %s; got %s" % (args[0], token), line, column)
        return token

    def header(self):
        self.expect("vid:pid")
        self.vid = int(self.expect("word"), 16)
        self.expect(":")
        self.pid = int(self.expect("word"), 16)
        self.expect("report_id")
        self.report_id = int(self.expect("byte"), 16)

    def control(self):
        self.kind.append(self.expect("left", "right", "forward", "backward", "fire"))
        self.expect("[")
        self.offset.append(int(self.expect("byte"), 16))
        self.expect("]")
        self.expect("&")
        self.mask.append(int(self.expect("byte"), 16))
        self.op.append(self.expect("=", "!=", "<", ">"))
        self.value.append(int(self.expect("byte"), 16))

    def parse(self):
        while not self.lexer.eof():
            self.header()
            self.kind = []
            self.offset = []
            self.mask = []
            self.op = []
            self.value = []
            while True:
                token = self.expect("left", "right", "forward", "backward", "fire", "vid:pid", "eof")
                if token in ["left", "right", "forward", "backward", "fire"]:
                    self.lexer.go_back()
                    self.control()
                elif token == "vid:pid":
                    self.lexer.go_back()
                    break
                elif token is None: # eof
                    break
                else:
                    raise "unexpected token: %s" % repr(token)
            #print self.vid, self.pid, self.report_id, self.kind, self.offset, self.mask, self.op, self.value
            self.data.append(6 + 4 * len(self.kind))
            self.data.append(self.vid & 0xff)
            self.data.append((self.vid >> 8) & 0xff)
            self.data.append(self.pid & 0xff)
            self.data.append((self.pid >> 8) & 0xff)
            self.data.append(self.report_id)
            for i in range(len(self.kind)):
                self.data.append({"left": 0x10, "right": 0x20, "forward": 0x30, "backward": 0x40, "fire": 0x50}[self.kind[i]] | {"=": 0x01, "!=": 0x02, "<": 0x03, ">": 0x04}[self.op[i]])
                self.data.append(self.offset[i])
                self.data.append(self.mask[i])
                self.data.append(self.value[i])
        self.data.append(0)  # append end-of-list marker


if len(sys.argv) != 3:
    print "usage: python %s IN-FILE OUT-FILE" % sys.argv[0]
    sys.exit(1)

with open(sys.argv[1], "rt") as f:
    text = f.read()
parser = Parser(Lexer(text))
try:
    parser.parse()
except ParseError, e:
    sys.stderr.write(str(e) + "\n")
    sys.exit(1)
with open(sys.argv[2], "wb") as f:
    f.write("".join(map(chr, parser.data)))
