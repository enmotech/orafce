/*----------------------------------------------------------------------------
 *
 *     nvarchar2.c
 *     NVARCHAR2 type for PostgreSQL.
 *
 *----------------------------------------------------------------------------
 */

#include "postgres.h"
#include "access/hash.h"
#include "access/tuptoaster.h"
#include "libpq/pqformat.h"
#include "nodes/nodeFuncs.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "mb/pg_wchar.h"
#include "fmgr.h"

#include "orafce.h"
#include "builtins.h"


PG_FUNCTION_INFO_V1(nvarchar2in);
PG_FUNCTION_INFO_V1(nvarchar2out);
PG_FUNCTION_INFO_V1(nvarchar2);
PG_FUNCTION_INFO_V1(nvarchar2recv);

/*
 * nvarchar2_input -- common guts of nvarchar2in and nvarchar2recv
 *
 * s is the input text of length len (may not be null-terminated)
 * atttypmod is the typmod value to apply
 *
 * If the input string is too long, raise an error
 *
 * Uses the C string to text conversion function, which is only appropriate
 * if VarChar and text are equivalent types.
 */

static VarChar *
nvarchar2_input(const char *s, size_t len, int32 atttypmod)
{
	VarChar		*result;		/* input data */
	size_t		maxlen;

	maxlen = atttypmod - VARHDRSZ;

	/*
	 * Perform the typmod check; error out if value too long for NVARCHAR2
	 */
	if (atttypmod >= (int32) VARHDRSZ && len > maxlen)
	{
		/* Verify that input length is within typmod limit.
		 *
		 * NOTE: blankspace is not truncated
		 */
		size_t		mbmaxlen = pg_mbstrlen(s);

		if (mbmaxlen > maxlen)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("input value length is %zd; too long for type nvarchar2(%zd)", mbmaxlen , maxlen)));
	}

	result = (VarChar *) cstring_to_text_with_len(s, len);
	return  result;
}

/*
 * Converts a C string to NVARCHAR2 internal representation.  atttypmod
 * is the declared length of the type plus VARHDRSZ.
 */
Datum
nvarchar2in(PG_FUNCTION_ARGS)
{
	char	*s = PG_GETARG_CSTRING(0);
#ifdef NOT_USED
	Oid		typelem = PG_GETARG_OID(1);
#endif
	int32	atttypmod = PG_GETARG_INT32(2);
	VarChar	*result;

	result = nvarchar2_input(s, strlen(s), atttypmod);
	PG_RETURN_VARCHAR_P(result);
}


/*
 * converts a NVARCHAR2 value to a C string.
 *
 * Uses the text to C string conversion function, which is only appropriate
 * if VarChar and text are equivalent types.
 */
Datum
nvarchar2out(PG_FUNCTION_ARGS)
{
	Datum   txt = PG_GETARG_DATUM(0);

	PG_RETURN_CSTRING(TextDatumGetCString(txt));
}

/*
 * converts external binary format to nvarchar
 */
Datum
nvarchar2recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);

#ifdef NOT_USED
	Oid			typelem = PG_GETARG_OID(1);
#endif
	int32		atttypmod = PG_GETARG_INT32(2);	/* typmod of the receiving column */
	VarChar		*result;
	char		*str;							/* received data */
	int			nbytes;							/* length in bytes of recived data */

	str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
	result = nvarchar2_input(str, nbytes, atttypmod);
	pfree(str);
	PG_RETURN_VARCHAR_P(result);
}


/*
 * nvarchar2send -- convert nvarchar2 to binary value
 *
 * just use varcharsend()
 */

/*
 * nvarchar2_transform()
 * Flatten calls to varchar's length coercion function that set the new maximum
 * length >= the previous maximum length.  We can ignore the isExplicit
 * argument, since that only affects truncation cases.
 *
 * just use varchar_support()
 */

/*
 * Converts a NVARCHAR2 type to the specified size.
 *
 * maxlen is the typmod, ie, declared length plus VARHDRSZ bytes.
 * isExplicit is true if this is for an explicit cast to nvarchar2(N).
 *
 * Truncation rules: for an explicit cast, silently truncate to the given
 * length; for an implicit cast, raise error if length limit is exceeded
 */
Datum
nvarchar2(PG_FUNCTION_ARGS)
{
	VarChar		*source = PG_GETARG_VARCHAR_PP(0);
	int32		typmod = PG_GETARG_INT32(1);
	bool		isExplicit = PG_GETARG_BOOL(2);
	int32		len,
				maxlen;
	int		maxmblen;
	char		*s_data;

	len = VARSIZE_ANY_EXHDR(source);
	s_data = VARDATA_ANY(source);
	maxlen = typmod - VARHDRSZ;

	/* No work if typmod is invalid or supplied data fits it already */
	if (maxlen < 0 || len <= maxlen)
		PG_RETURN_VARCHAR_P(source);

	/* only reach here if string is too long... */

	/* truncate multibyte string preserving multibyte boundary */
	maxmblen = pg_mbcharcliplen(s_data, len, maxlen);

	/* error out if value too long unless it's an explicit cast */
	if (!isExplicit)
	{
		/* if there is still data beyond maxmblen, error out
		 *
		 * Remember - no blankspace truncation on implicit cast
		 */
		if (len > maxmblen)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("input value too long for type nvarchar2(%d)", maxlen)));
	}

	PG_RETURN_VARCHAR_P((VarChar *) cstring_to_text_with_len(s_data, maxmblen));
}


/*
 * nvarchar2typmodin -- type modifier input function
 *
 * just use varchartypmodin()
 */

/*
 * nvarchar2typmodout -- type modifier output function
 *
 * just use varchartypmodout()
 */
