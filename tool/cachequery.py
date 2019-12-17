#!/usr/bin/env python3

from lark import Lark, Transformer, v_args
from functools import reduce
from datetime import datetime
import os, cmd, sys, getopt, re, subprocess, configparser
import plyvel

LOG_HEADER_TEMPLATE = '''====================================
CacheQuery log - {}
===================================='''

class CacheQuery():

    def __init__(self, conf):
        self.settings = conf['General']
        self.system = conf['System']
        self.conf = conf

        # open db from beggining
        if self.settings['db_cache']:
            self.db = plyvel.DB(self.settings['db_cache'], create_if_missing=True)
        else:
            self.db = None

        try:
            self.check_system()
            self.disable_noise()
            pass
        except:
            print("[!] Error: invalid system settings")
            sys.exit()

    def check_system(self):
#        if not os.access('/sys/kernel/cachequery', os.R_OK):
#                print("err: cachequery.ko")
#                raise
        if self.system.getboolean('disable_prefetch') or self.system.getboolean('disable_turboboost'):
            try:
                if os.system('sudo modprobe msr') != 0:
                    print("err: modprobe msr")
                    raise
            except:
                print("err: exec modprobe msr")
                raise
        if self.system.getint('frequency_set') > 0:
            try:
                if os.system('sudo modprobe acpi-cpufreq') != 0:
                    print("err: modprobe acpi-cpufreq")
                    raise
            except:
                print("err: modprobe acpi-cpufreq")
                raise

    def _exec(self, command):
        p=subprocess.Popen(command, shell=True, stdout=subprocess.DEVNULL)
        p.wait()
        return p.returncode

    def disable_noise(self):
        try:
            if self.system.getboolean('disable_ht'):
                if self._exec('echo 0 | sudo tee /sys/devices/system/cpu/cpu*/online') == 0:
                    print(" [debug] disabling hyperthreading and multi-core")
                else:
                    print(" ERR: hyperthreads", file=sys.stderr)
            if self.system.getboolean('disable_prefetch'):
                if self._exec('sudo wrmsr -a 0x1a4 15') == 0:
                    print(" [debug] disabling hardware prefetchers")
                else:
                    print(" ERR: hw prefetch", file=sys.stderr)
            if self.system.getboolean('disable_turboboost'):
                if self._exec('sudo wrmsr -a 0x1a0 0x4000850089') == 0:
                    print(" [debug] disabling intel's turboboost")
                else:
                    print(" ERR: turboboost", file=sys.stderr)
            freq = self.system.getint('frequency_set')
            if freq > 100:
                if self._exec('sudo cpupower frequency-set -d {}MHz'.format(freq)) == 0 and self._exec('sudo cpupower frequency-set -u {}MHz'.format(freq)) == 0 and self._exec('sudo cpupower frequency-set -g performance') == 0:
                    print(" [debug] fixing cpu frequency")
                else:
                    print(" ERR: fix freq", file=sys.stderr)
        except:
            raise

    def reenable_noise(self):
        try:
            if self.system.getboolean('disable_prefetch'):
                if self._exec('sudo wrmsr -a 0x1a4 0') != 0:
                    print(" ERR: hw prefetch", file=sys.stderr)
            if self.system.getboolean('disable_turboboost'):
                if self._exec('sudo wrmsr -a 0x1a0 0x850089') != 0:
                    print(" ERR: turboboost", file=sys.stderr)
            if int(self.system['frequency_set']) > 100:
                if self._exec('sudo cpupower frequency-set -d 1MHz') != 0 or self._exec('sudo cpupower frequency-set -u 5000MHz') != 0 or self._exec('sudo cpupower frequency-set -g powersave') != 0:
                    print(" ERR: fix freq")
            if self.system.getboolean('disable_ht'):
                if self._exec('echo 1 | sudo tee /sys/devices/system/cpu/cpu*/online') != 0:
                    print(" ERR: hyperthreads", file=sys.stderr)
        except:
            raise

    def parse(self, input):

        memblock_grammar = '''
        ID: /[a-zA-Z]+/
        QUESTION: "?"
        FLUSH: "!"
        NUMBER: /[0-9]+/

        SPACE: (" " | /\t/ )+

        AT: "@"
        WILDCARD: "_"
        INVD: "!"

        flag: (QUESTION | FLUSH)        -> string

        block: ID                       -> trans

        addresses: block                -> first
            | addresses SPACE block     -> append

        group: block                    -> singleton
            | "(" addresses ")"         -> group
            | "[" addresses "]"         -> parallel
            | AT                        -> expand
            | WILDCARD                  -> unfold

        query: group flag?              -> modify
            | group NUMBER flag?        -> modify_power
            | INVD                      -> invalidate

        trace: query                    -> list
            | trace SPACE query         -> concat

        start: trace                    -> out

        '''

        class MemBlockToQuery(Transformer):

            def __init__(self, alfa, conf, debug=False):
                self.dir = []
                self.alphabet = alfa
                self.ways = int(conf.cache('ways'))
                self.cacheset = int(conf.cache('set'))
                self.debug = debug

            def print(self, *args):
                if self.debug:
                    print(*args)

            # Cast Tree to string
            @v_args(inline=True)
            def string(self, _):
                return str(_)

            # Cast Tree to list
            @v_args(inline=True)
            def list(self, _):
                self.print("list: ", list(_))
                return list(_)

            # First
            @v_args(inline=True)
            def first(self, _):
                self.print("first: ", [_])
                return [_]

            # Propagate identity
            @v_args(inline=True)
            def id(self, _):
                self.print("id: ", _)
                return _

            # Returns block
            @v_args(inline=True)
            def trans(self, block):
                id=str(block)
                try:
                    ret = str(self.dir.index(id))
                except ValueError:
                    self.dir.append(id)
                    ret = str(len(self.dir)-1)
                self.print("block: ", block, ret)
                return ret

            # Appends block into list
            @v_args(inline=True)
            def append(self, addresses, _, block):
                self.print("append: ", addresses, block)
                addresses.append(block)
                return addresses

            # Returns group of lists
            @v_args(inline=True)
            def group(self, addresses):
                self.print("group: ", [addresses])
                return [addresses]

            # Returns group of lists
            @v_args(inline=True)
            def singleton(self, addresses):
                self.print("singleton: ", [[addresses]])
                return [[addresses]]

            # Returns group of lists
            @v_args(inline=True)
            def invalidate(self, _):
                self.print("invalidate")
                return _

            # Returns group of groups
            @v_args(inline=True)
            def parallel(self, addresses):
                ret = [[g] for g in addresses]
                self.print("parallel: ", ret)
                return ret

            # Expands '@' into query of assoc blocks
            @v_args(inline=True)
            def expand(self, _):
                tmp = self.alphabet[:self.ways]
                ret = []
                for i in tmp:
                    ret.append(self.trans(i))
                ret = self.group(ret)
                self.print("expand: ", ret)
                return ret

            # Unfolds '_' into assoc blocks
            @v_args(inline=True)
            def unfold(self, _):
                names = self.alphabet[:self.ways]
                ret = []
                for e in names:
                    try:
                        ret.append(str(self.dir.index(e)))
                    except ValueError:
                        self.dir.append(e)
                        ret.append(str(len(self.dir)-1))
                ret = self.parallel(ret)
                self.print("unfold: ", ret)
                return ret

            # Adds flag to groups
            @v_args(inline=True)
            def modify(self, groups, flag=''):
                ret = []
                for g in groups:
                    if isinstance(g, str):
                        ret.append(g + flag)
                    else:
                        ret.append([(e + flag) for e in g])
                ret = [' '.join(g) for g in ret]
                self.print("modify: ", ret)
                return ret

            # Applies power and modifies flag to groups
            @v_args(inline=True)
            def modify_power(self, groups, n, flag=''):
                ret = self.modify(groups, flag) * int(n)
                ret = [' '.join(ret)]
                self.print("modify_power: ", ret)
                return ret

            # Converts groups into strings
            @v_args(inline=True)
            def join(self, queries):
                ret = [' '.join(queries)]
                self.print("join: ", ret)
                return ret

            # Cartesian product of groups
            @v_args(inline=True)
            def concat(self, traces, _, queries):
                ret = [(x+" "+y) for x in traces for y in queries]
                self.print("concat: ", ret)
                return ret


            # Flatten final list of queries
            @v_args(inline=True)
            def out(self, traces):
                # append cacheset number to block ids
                ret = [[re.sub(r'(\d+)', r'\1_0', b) for b in t.split()] for t in traces]
                return [traces, [' '.join(q) for q in ret]]


        memblock_parser = Lark(memblock_grammar, parser='lalr',  transformer=MemBlockToQuery(self.settings['alphabet'], self.conf, debug=False))
        parser = memblock_parser.parse

        try:
            parser_query = parser(input)
            return parser_query
        except Exception:
            print("syntax error", file=sys.stderr)

    def query(self, query, refresh=True):
        path = '/sys/kernel/cachequery/{}_sets/{}/run'.format(self.settings['level'].lower(), self.conf.cache('set'))
        try:
            if refresh:
                with open(path, 'w') as endpoint:
                    endpoint.write('{}\n'.format(query))
                    endpoint.close()
            with open(path, 'r') as endpoint:
                res = endpoint.readline()
                endpoint.close()
                return res.rstrip()
        except:
            raise

    def command(self, input):
        print('({}:{}) r {}'.format(self.settings['level'], self.conf.cache('set'), input))
        self.run(input)

    def run(self, input, bypass_cache=False, refresh=True):
        try:
            query, query_raw = self.parse(input)
        except:
            return
        answer = []
        for i in range(len(query)):
            q_raw = query_raw[i]
            q = query[i]
            ret = ''
            # Check cache to safe avoid real query (only for deterministic policies)
            if self.db and not bypass_cache:
                ret = self.db.get(str.encode(q))
            if not ret:
                ret = self.query(q_raw, refresh)
                if self.db:
                    self.db.put(str.encode(q), str.encode(ret))
            else:
                ret = ret.decode('utf-8')
            print('{} -> {}'.format(q, ret))
            answer.append('{} -> {}'.format(q, ret))
        return answer

    def interactive(self):
        cq = self

        class CQShell(cmd.Cmd):

            intro = 'CacheQuery interactive shell. Type "help" or "?" to list commands.\n'
            prompt = '({}:{}) '.format(cq.settings['level'], cq.conf.cache('set'))
            response = None
            if cq.settings['log_file']:
                file = open(cq.settings['log_file'], 'a')
                print(LOG_HEADER_TEMPLATE.format(datetime.now().strftime("%Y/%m/%d %H:%M:%S")), file=file)
            else:
                file = None

            def do_r(self, line):
                'Execute query. Example: r @ M _? (default, `r` is optional)'
                self.response = [cq.run(line)]

            def default(self, line):
                self.response = [cq.run(line)]

            def do_rr(self, line):
                'Execute query bypassing cache if present. Example: rr @ M _?'
                self.response = [cq.run(line, True)]

            def do_set(self, line):
                'Overwrite target cacheset. Example: set 12'
                try:
                    if int(line) > -1:
                        cq.conf.set_cache('set', line)
                except:
                    pass
                # refresh prompt
                self.prompt = '({}:{}) '.format(cq.settings['level'], cq.conf.cache('set'))

            def do_level(self, line):
                'Overwrite target level. Example: level L1'
                if re.match(r'^L(3|2|1)$', line, re.IGNORECASE) is None:
                    pass
                else:
                    cq.settings['level'] = line.upper()
                # refresh prompt
                self.prompt = '({}:{}) '.format(cq.settings['level'], cq.conf.cache('set'))

            def do_R(self, line):
                'Repeat query `config.repeat` times. Example: R @ M N b?'
                self.response = [cq.run(line, False, True)] # first to generate code
                for i in range(cq.settings.getint('repeat')):
                    self.response.append(cq.run(line, True, cq.settings.getboolean('refresh'))) # bypass cache

            # log session
            def do_log(self, arg):
                'Log session into file. Example: log test.cqlog'
                if not self.file:
                    self.file = open(arg, 'a')
                    print(LOG_HEADER_TEMPLATE.format(datetime.now().strftime("%Y/%m/%d %H:%M:%S")), file=file)
            def do_unlog(self, arg):
                'Stop log session'
                self.close()
            def close(self):
                if self.file:
                    self.file.close()
                    self.file = None
                if cq.db:
                    cq.db.close()
                    cq.db = None

            # execute before any command, write into session log file
            def precmd(self, line):
                if self.file:
                    print('{} {}'.format(self.prompt, line), file=self.file)
                return line
            # execute after command, write into session log file
            def postcmd(self, stop, line):
                if self.file and self.response:
                    for answers in self.response:
                        for r in answers:
                            print(r, file=self.file)
                    self.response = None
                return stop

            # exit commands
            def do_EOF(self, line):
                'Quit'
                self.close()
                return True

            def do_q(self, line):
                'Quite'
                self.close()
                return True

        return CQShell()

def usage():
    help = '''
    [!] ./cachequery [options] <query>

    Options:
        -h --help
        -i --interactive
        -v --verbose

        -c --config=filename    path to filename with config (default: 'cachequery.ini')
        -b --batch              path to filename with list of commands
        -o --output             path to output file for session log

        -l --level              target cache level: L3|L2|L1
        -s --set                target cache set number

    '''
    print(help)

def main():

    try:
        opts, args = getopt.getopt(sys.argv[1:], "ho:c:ivb:s:l:", ["help", "output=", "config=", "interactive", "batch=","set=","level="])
    except getopt.GetoptError as err:
        print(str(err))
        usage()
        sys.exit(2)

    # flags
    output = None
    verbose = False
    interactive = False
    # options
    config_path = 'cachequery.ini' # default path
    batch = None

    # config overwrite
    cacheset = None
    level = None

    # TODO: allow overwrite some config values
    for o, a in opts:
        if o == "-v":
            verbose = True
        elif o in ("-h", "--help"):
            usage()
            sys.exit()
        elif o in ("-c", "--config"):
            config_path = a
        elif o in ("-i", "--interactive"):
            interactive = True
        elif o in ("-b", "--batch"):
            batch = a
        elif o in ("-o", "--output"):
            output = a
        elif o in ("-l", "--level"):
            if re.match(r'^L(3|2|1)$', a, re.IGNORECASE) is None:
                assert False, "invalid level"
            else:
                level = a.upper()
        elif o in ("-s", "--set"):
            try:
                if int(a) > -1:
                    cacheset = a
            except:
                assert False, "invalid cacheset"
        else:
            assert False, "unhandled option"

    # read config
    try:
        config = configparser.ConfigParser()
        config.read(config_path)
        # add method for dynamic cache check
        def cache(self, prop):
            return self.get(self.get('General', 'level'), prop)
        def set_cache(self, prop, val):
            return self.set(self.get('General', 'level'), prop, val)
        setattr(configparser.ConfigParser, 'cache', cache)
        setattr(configparser.ConfigParser, 'set_cache', set_cache)
    except:
        print("[!] Error: invalid config file")
        sys.exit(1)

    # overwrite options
    if level:
        config.set('General', 'level', level)
    if cacheset:
        config.set_cache('set', cacheset)
    if output:
        config.set('General', 'log_file', output)

    # instantiate cq
    CQ = CacheQuery(config)

    # exec single command
    if not interactive and not batch:
        if len(args) < 1:
            usage()
            sys.exit(2)
        query = ' '.join(args)
        CQ.command(query)

    # run batch of commands from file
    elif batch:
        try:
            with open(batch, 'r') as endpoint:
                query = endpoint.readline().rstrip()
                CQ.command(query)
            endpoint.close()
        except:
            print("[!] Err: invalid filename")

    # start interactive mod
    else:
        CQ.interactive().cmdloop()

    # destroyy
    CQ.reenable_noise()

if __name__ == '__main__':
    main()
