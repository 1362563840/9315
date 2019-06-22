# 9315

liu is doing string check

dynamica struct string is done

string check is almost finished

one problem, when insert something, then exit, then reconnect databse, then "select * from table", error will shows, do not know why

if bugs says "wrong ELF class: ELFCLASS64", it means that you need to "make" in grieg.

if bug says "errstart was not found", it means that some includes are missing

if bugs says "server closed the connection unexpectedly
	This probably means the server terminated abnormally
	before or while processing the request.
The connection to the server was lost. Attempting reset: Failed.
"
it means that you are likely to modify memory that you should not change. I.E. your pointer may point wrony memory. Or you are tring 
to assign value that is already freed or otherwist
