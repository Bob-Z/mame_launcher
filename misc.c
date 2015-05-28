#include <stdlib.h>
#include <string.h>

/******************************
 * Return the temporay directory path
 *
 * Result MUST be freed
 ******************************/
char * get_tmp_dir()
{
	char * tmp;

	tmp = getenv("TMP");
	if( tmp == NULL ) {
		tmp = getenv("TMPDIR");
		if( tmp == NULL ) {
			tmp = getenv("TEMP");
			if( tmp == NULL ) {
				tmp = getenv("TEMPDIR");
				if( tmp == NULL ) {
					tmp = "/tmp";
				}
			}
		}
	}

	return strdup(tmp);
}
