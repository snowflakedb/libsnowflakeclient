import re
import enum
from dataclasses import dataclass
from sys import stderr
from typing import Optional, List
import dataclasses
import json
import argparse

"""
Regexes for matching warnings:
/tmp/libsnowflakeclient/include/snowflake/SF_CRTFunctionSafe.h:223:16: warning: unused parameter 'in_sizeOfBuffer' [-Wunused-parameter]     <- message
  223 |         size_t in_sizeOfBuffer,                                                                                                     <- snippet
"""
message_re = re.compile(r"""(?P<file_path>[^:]*):(?P<line>\d+):(?:(?P<col>\d+):)?\s+warning:(?P<message>.*)\[(?P<flag>.*)\]""")

class ParserState(enum.Enum):
    MESSAGE = "MESSAGE"
    SNIPPET = "SNIPPET"

@dataclass
class CompilerWarning:
    file_path: str
    line: int
    column: Optional[int]
    message: str
    flag: str
    snippet: Optional[str]
    source: str
    def key(self):
        return self.file_path, self.message, self.flag, self.snippet

@dataclass
class WarningDiff:
    new: List[CompilerWarning]
    old: List[CompilerWarning]

"""
Parses warnings from compiler output
"""
def parse_warnings(path: str) -> List[CompilerWarning]:
    warnings = []
    state = ParserState.MESSAGE
    with open(path, "r") as f:
        lines = f.readlines()
        for line in lines:
            if state == ParserState.MESSAGE:
                m_match = message_re.match(line)
                if not m_match:
                    continue

                col = m_match.group("col")
                if col:
                    col = int(col)

                warning = CompilerWarning(
                    file_path=m_match.group("file_path"),
                    line=int(m_match.group("line")),
                    column=col,
                    message=m_match.group("message"),
                    flag=m_match.group("flag"),
                    snippet=None,
                    source=line
                )

                warnings.append(warning)
                state = ParserState.SNIPPET
                continue

            if state == ParserState.SNIPPET:
                warning.snippet = line
                warning.source += line

                state = ParserState.MESSAGE
                continue

        return warnings

def dump_warnings(warnings: List[CompilerWarning]) -> str:
    warnings_as_dict = [dataclasses.asdict(w) for w in warnings]
    return json.dumps(warnings_as_dict, indent=2)

def load_warnings(warnings_json: str) -> List[CompilerWarning]:
    warnings_as_dict = json.loads(warnings_json)
    return [CompilerWarning(**w) for w in warnings_as_dict]

def write(path: str, data: str):
    with open(path, "w") as f:
        f.write(data)

def read(path: str) -> str:
    with open(path, "r") as f:
        return f.read()

def generate_report(path: str, new_warnings: List[CompilerWarning], old_warnings: List[CompilerWarning]):
    with open(path, "w") as f:
        diff = {}
        for nw in new_warnings:
            if nw.key() not in diff:
                diff[nw.key()] = WarningDiff(new=[], old=[])

            diff[nw.key()].new.append(nw)

        for ow in old_warnings:
            if ow.key() not in diff:
                diff[ow.key()] = WarningDiff(new=[], old=[])

            diff[ow.key()].old.append(ow)

        for d in diff.values():
            balance = len(d.new) - len(d.old)
            if balance < 0:
                f.write("Removed {} compiler warnings from {} [{}].\n".format(-balance, d.old[0].file_path, d.old[0].flag))

            if balance > 0:
                f.write("Added {} compiler warnings to {} [{}]. Please remove following warnings:\n".format(balance, d.new[0].file_path, d.new[0].flag))
                for w in d.new:
                    f.write(w.source)

parser = argparse.ArgumentParser(
    prog='generate_warning_report',
    description='Generate compiler warning report',
    epilog='Text at the bottom of help'
)

parser.add_argument('--build-log', required=True)      # option that takes a value
parser.add_argument('--load-warnings', required=True)
parser.add_argument('--dump-warnings', required=True)
parser.add_argument('--report', required=True)
args = parser.parse_args()

new_warnings = parse_warnings(args.build_log)
old_warnings = load_warnings(read(args.load_warnings))
generate_report(args.report, new_warnings, old_warnings)
write(args.dump_warnings, dump_warnings(new_warnings))

