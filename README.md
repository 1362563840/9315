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


warning 
"
If the internal representation of the data type is variable-length, the internal representation must follow the standard layout for variable-length data: the first four bytes must be a char[4] field which is never accessed directly (customarily named vl_len_). You must use the SET_VARSIZE() macro to store the total size of the datum (including the length field itself) in this field and VARSIZE() to retrieve it. (These macros exist because the length field may be encoded depending on platform.)
"

warning 
in email.source
This inequality, check if restrict and join is correct
one possible fault is "
CREATE OPERATOR <> (
   leftarg = EmailAddr, rightarg = EmailAddr, procedure = email_abs_inequality,
   commutator = <> ,
   negator = = ,
   restrict = eqsel, join = eqjoinsel
);
"

warning
in email.source. Check if one new operator "<>" is inserted correct
one possible fault about btree is "
-- now we can make the operator class
CREATE OPERATOR CLASS complex_abs_ops
    DEFAULT FOR TYPE complex USING btree AS
        OPERATOR        1       < ,
        OPERATOR        2       <= ,
        OPERATOR        3       = ,
        OPERATOR        4       = ,
        OPERATOR        5       >= ,
        OPERATOR        6       > ,
        FUNCTION        1       complex_abs_cmp(complex, complex);
"
