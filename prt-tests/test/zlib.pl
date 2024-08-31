# PRT agents to build zlib in the lmcas container

our @build_zlib = (
    echo("Zlib: starting configure"),
    {
        name         => "configure-zlib",
        command      => [ "docker","exec","lmcas","bash","-c","tar xzf /usr/src/zlib-1.2.12.tar.gz; cd zlib-1.2.12; ./configure",
                          ],
        comparo      => undef,
        complete_before_continuing =>1,
    },
    echo("Zlib: starting make"),
    {
        name         => "make-zlib",
        command      => [ "docker","exec","lmcas","bash","-c",'cd zlib-1.2.12; make -j $(nproc) -l $(nproc)',
                          ],
        comparo      => undef,
        complete_before_continuing =>1,
    },
    echo("Zlib: starting install"),
    {
        name         => "install-zlib",
        command      => [ "docker","exec","lmcas","bash","-c",'cd zlib-1.2.12; make install -j $(nproc) -l $(nproc)',
                          ],
        comparo      => undef,
        complete_before_continuing =>1,
    },
    {
        name         => "check-zlib",
        command      => [ "docker","exec","lmcas","bash","-c","test -f /usr/local/lib/libz.a"
                          ],
        comparo      => undef,
        complete_before_continuing =>1,
    },
    echo("Zlib: built and installed successfully"),
);

1;   # leave this at end of file to tell perl it loaded OK
