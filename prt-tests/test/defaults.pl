# Should be used at top of every test to reset default values, since the perl will carry them along from prior tests (sorry, icky).
# In earlier versions we were having files only set default values if there wasnt already a defined value, but that pattern is
# flawed because tests run in the same perl as the prior test.

do "$ENV{'PRT_PATH'}/common.pl" || die "Can't do '$ENV{'PRT_PATH'}/common.pl': $@";

use PRT::Perf qw(smaller_is_better bigger_is_better
                 numeric_in_range same_number);

1;	# leave this here at the very end, so 'do'ing this file succeeds
