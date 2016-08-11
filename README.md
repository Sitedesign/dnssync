# dnssync
This is a DNS synchronization solution for DJBDNS with DDNS support.

There two main parts in this:
* **dnssync** - the sync logic
* **dnssync-web** - the DDNS frontend daemon

Both service running in the daemontools environment.

## Requirements

dnssync depends on:
* daemontools
* daemontools-run
* djbdns

## dnssync

The dnssync is synchronization daemon which periodically checks local and remote serials for the configured zones. If a zone serial updated a zone transfer will begin. Written in Bash.

### Configuration

#### Environment variables

* DDNS - Enable DDNS sync *(value: 0 or 1, default: 1)*
* DDNSURL - URL for retriving the DDNS zone, this the dnssync-web URL *(default: http://test.domain.tld/dnssync/retrive/)*
* PERIOD - Checking period in seconds *(default: 300)*
* ROOT - Root directory path for dnssync *(default: /var/dns/dnssync/root)*
* TIMEOUT - Timeout for checking remote serial in seconds *(default: 5)*
* TINYDNSDIR - Local TinyDNS root directory / zones directory *(default: /var/dns/tinydns/root/primary)*
* TMPDIR - Directory path for temporary files *(default: /tmp)*
* UPDATESCRIPT - This is a path for a script which makes data.cdb for TinyDNS *(default: /usr/local/bin/dnssync_update)*
* ZTMETHOD - Zone transfer method - only axfr implemented *(default: axfr)*

#### Root structure

Root directory example for better understanding:
```
.
./adns
./adns/ns1.domain.tld
./adns/ns1.domain.tld/testdomain.tld
./adns/ns2.domain.tld
./adns/ns2.domain.tld/testdomain.tld
./ddns
```

Adns directory means authoritative dns. In this directory other directories have to be created with DNS servers' name.
Under the DNS server directories files have to be created with domain names which wanted to synchronized. The initial
content of the domain files must be 0. Dnssync stores zone serial there.

Ddns directory: not implemented yet.

#### Update script

The update script path defined in the environment variables. This script is used to execute functions when zone need to be updated (and this script updates the zone under TinyDNS):

```bash
#!/bin/bash
/var/dns/tinydns/root/make.sh
svc -h /service/dnscache
```

## dnssync-web

This is a webserver for updating IPs for DDNS domains and retriving DDNS zone. This is the modified version of tinyhttpd written in C.

### Configuration

#### Environment variables

* IP - binding IP *(default: 127.0.0.1)*
* PORT - binding port *(default: 80)*
* ROOT - Root directory path for dnssync-web *(default: /var/dns/dnssync-web/root)*
* THREADMAX - Maximum number of concurrent threads *(default: 10)*

#### Root structure

Root directory example for better understanding:

```
.
./clients
./clients/john.ddns.domain.tld
./clients/john.ddns.domain.tld/IP
./clients/john.ddns.domain.tld/TTL
./clients/john.ddns.domain.tld/KEY
./servers
./servers/ns1.domain.tld
./servers/ns2.domain.tld
./servers/ns3.domain.tld
```

There are two main directories under root: clients and server.

Under clients directory we define the DDNS client domains as a directory. Under each client there are three files: IP, TTL, KEY:
* The IP file stores the current IP address of the client *(default: 127.0.0.1)*
* The TTL file stores the Time To Live value of the IP in seconds *(default: 60)*
* The KEY file stores a key value for the client, which determines the client at IP update HTTP calls. The key should be a  string of letters and numbers.

Under servers directory we define files with names of the name servers which allowed to retrive the DDNS zone. The files should be empty.

### Updating client IP

The client IP can be updated by a simple HTTP GET request to the dnssync-web server. For example:
```
http://ns2.domain.tld/dnssync/5BEKrNynsaZ4Xwy/
```
where 5BEKrNynsaZ4Xwy is the KEY of the john.ddns.domain.tld.

### Retriving DDNS zone

DDNS can be retrived by a simple HTTP GET request from the allowed servers to the dnssync-web server. For example:
```
http://ns2.domain.tld/dnssync/retrive/
```
The response is in TinyDNS format.
