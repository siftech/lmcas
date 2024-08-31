# PRT agents to run and test lmcas

use PRT::Perf qw(smaller_is_better bigger_is_better
                 numeric_in_range same_number);

our @start_lmcas_container = (
    echo("Starting LMCAS container"),
    {
        name         => "start-lmcas",
        command      => [ "../tools/start-image","lmcas-deliv", "--name", "lmcas",
                          "-v", sub { my $agent=shift; return $agent->testspec->results_dir . ":/logdir";},
                          "-e", sub { my $agent=shift; return "OUTER_PRT_RESULT_DIR=" . $agent->testspec->results_dir;},
			  "--mount type=bind,src=$ENV{'ALT_WGET_SPEC'},dst=/specs/wget.json",
			  "-d",
                          ],
        comparo      => undef,
        complete_before_continuing =>1,
    },
#    {
#        name         => "lmcas-container-stays-alive",
#        command      => [ "../tools/docker-stays-alive", "lmcas"], 
#        comparo      => undef,
#        daemon =>1,
#        must_survive => 1,
#        exit_code    => undef,
#        complete_before_continuing =>0,
#    }
);


our @build_main = (
    echo("Building main"),
    {
        name         => "build-main",
        command      => [ "docker","exec","lmcas","bash","-c","cd example; musl-gclang main.c -o main",
                          ],
        comparo      => undef,
        complete_before_continuing =>1,
    },
);

1;   # leave this at end of file to tell perl it loaded OK
