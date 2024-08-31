# common agents shared by most/all tests

#use PRT::Perf qw(smaller_is_better bigger_is_better
#                 numeric_in_range same_number);

## -------------------------------- ## --------------------------------

$ENV{DEBUG}=1;

# Sharing
$ENV{SHELL_DONT_SHARE_EXPERIENCES}=$shell_dont_share_experiences;

# Comms loss + delay
$ENV{SHELL_MSG_GAME_DELAY}=$shell_msg_game_delay;
$ENV{SHELL_MSG_DROP_THRESHOLD}=$shell_msg_drop_threshold;
$ENV{RANDOM_SEED}=$random_seed;

# Other 
$ENV{AGENT_ENV_FILE}=$agent_env_file;
$ENV{ALLOCATION_METHOD}=$allocmethod;

# When set, include check-for-ommm-errors agent (see below).
$testing = $ENV{TESTING};
if (!(defined $testing)) { $testing=0;}

sub check_for_pattern {	
    use Symbol qw(gensym);
    my ($logfile,$pattern) = @_;
    my $name=gensym;
    my @agent = ({
        name      => "check_for_pattern-$name",
        command   => sub {
            use POSIX qw(strftime);
            $now_string = strftime "%F %T", localtime;
            print "$now_string Checking $logfile for $pattern\n";
	    system("egrep",$pattern,$logfile);
            exit 0; },
        complete_before_continuing => 1,
        comparo => undef,
                 },);
    return @agent;
}

		#"/usr/bin/xterm", "-hold", "-e", "tail", "-f", "-s", "5", "$_[0]" ],
	#"/usr/bin/gnome-terminal",  "--", "tail", "-f", "-s", "5", "$_[0]" ],
sub xterm_tail() {
    (echo("Opening xterm window for $_[0]"),
    {
        name                => "xterm-$_[1]",
        command             => [ "cd-then-exec", sub {my $agent=shift; return $agent->testspec->results_dir;},
        "/usr/bin/xterm", "-geometry", "75x24+0+0","-fa","Monospace","-fs","18","-hold", "-e", "tail","-f","-n","+1","-s","5", "$_[0]"],
        comparo             => undef,
        exit_code             => undef,
        complete_before_continuing  => 0,
    }
    )};

our @dstat_agent = (
    { 
        name         => "dstat",
        command      => [ "$ENV{'PRT_PATH'}/timestamp", "dstat", "-tTlcsg", "--top-mem", "--mem-adv", "--noupdate", "--nocolor", "--output", sub {my $agent=shift; return ($agent->testspec->results_dir . "/dstat-output.csv");}, 10],
        comparo      => undef,
        daemon =>1,
        must_survive => 0,
        exit_code    => undef,
        complete_before_continuing =>0,
    });

1;
