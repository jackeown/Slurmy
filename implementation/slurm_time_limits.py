"""Read SLURM wall-time policy for a partition and the selected account."""
from __future__ import annotations
import argparse
import getpass
import json
import os
from pathlib import Path
import re
import subprocess
import time

_CACHE = {}


def seconds(value):
    value = str(value or '').strip()
    if value.upper() in {'', 'UNLIMITED', 'INFINITE', 'NONE', 'N/A', 'NOT_SET', '0', '00:00:00'}:
        return None
    if not re.fullmatch(r'\d+(?:-\d+)?(?::\d+){0,2}', value):
        raise ValueError('Unrecognized SLURM time limit: '+value)
    days, sep, rest = value.partition('-')
    parts = [int(part) for part in (rest if sep else value).split(':')]
    if sep:
        hours, minutes, secs = (parts+[0,0])[:3]
        return int(days)*86400+hours*3600+minutes*60+secs
    if len(parts)==1:return parts[0]*60
    if len(parts)==2:return parts[0]*60+parts[1]
    return parts[0]*3600+parts[1]*60+parts[2]


def evaluate(partition_info, associations, qos_info, account, user):
    fields = dict(re.findall(r'(\w+)=([^\s]+)', partition_info))
    if 'MaxTime' not in fields:
        raise ValueError('SLURM did not report the partition maximum time.')
    partition_limit = seconds(fields['MaxTime'])
    rows = [line.split('|') for line in associations.splitlines()]
    rows = [row for row in rows if len(row)>=6 and row[0]==account and row[1]==user and row[2] in ('', fields.get('PartitionName'))]
    specific = [row for row in rows if row[2]]
    rows = specific or rows
    qos = {}
    for line in qos_info.splitlines():
        row = line.split('|')
        if len(row)>=3:
            qos[row[0]] = (set(row[1].casefold().split(',')), seconds(row[2]))
    defaults = {row[3] for row in rows if row[3]}
    allowed = {name for row in rows for name in row[4].split(',') if name and name not in {'ALL','N/A'}}
    candidates = defaults or allowed or {''}
    association_limits = [seconds(row[5]) for row in rows if seconds(row[5]) is not None]
    association_limit = min(association_limits, default=None)
    partition_qos = qos.get(fields.get('QoS',''), (set(),None))
    limits = []
    override = True
    for name in candidates:
        flags, job_limit = qos.get(name, (set(),None))
        overrides = 'partitiontimelimit' in flags
        override &= overrides
        ordered = (job_limit, partition_qos[1]) if 'overpartqos' in flags else (partition_qos[1], job_limit)
        policy_limit = next((limit for limit in ordered if limit is not None), association_limit)
        applicable = [limit for limit in (policy_limit, None if overrides else partition_limit) if limit is not None]
        if applicable:limits.append(min(applicable))
    limit = min(limits, default=None)
    # SLURM accepts whole minutes; rounding UP could exceed the limit.
    if limit is not None:
        limit = limit//60*60
        if limit<60:raise ValueError('SLURM reports a wall-time limit shorter than one scheduling minute.')
    return dict(limit_seconds=limit, partition_limit_seconds=partition_limit,
                account=account, qos=','.join(sorted(candidates-{''})),
                partition_time_override=bool(override),
                policy_verified=bool(rows and len(candidates)==1 and candidates <= qos.keys()))


def _read(argv, required=False):
    try:
        result = subprocess.run(argv,capture_output=True,text=True,timeout=8,check=False)
        if result.returncode==0:return result.stdout
        detail = result.stderr.strip()
    except (OSError,subprocess.TimeoutExpired) as error:detail=str(error)
    if required:raise ValueError(detail or 'Could not read SLURM time limits.')
    return ''


def local_policy(partition, account='', partition_info=None):
    if not re.fullmatch(r'[A-Za-z0-9_.-]+',partition) or (account and not re.fullmatch(r'[A-Za-z0-9_.-]+',account)):
        raise ValueError('Invalid partition or account.')
    user = os.environ.get('USER') or getpass.getuser()
    if not account:
        output = _read(['sacctmgr','-nP','show','user',user,'format=User,DefaultAccount'])
        account = next((line.split('|')[1] for line in output.splitlines() if line.startswith(user+'|') and len(line.split('|'))>1),'default') or 'default'
    info = partition_info or _read(['scontrol','show','partition',partition,'-o'],required=True)
    associations = _read(['sacctmgr','-nP','show','assoc','user='+user,'account='+account,
                          'format=Account,User,Partition,DefaultQOS,QOS,MaxWall'])
    qos = _read(['sacctmgr','-nP','show','qos','format=Name,Flags,MaxWall'])
    return evaluate(info,associations,qos,account,user)


def query_remote(host, partition, account=''):
    """Editor-only short cache; actual submissions always query on the cluster."""
    from ssh_transport import connection_options
    import shlex
    for value in (host,partition,account or 'default'):
        if not re.fullmatch(r'[A-Za-z0-9_.-]+',value):raise ValueError('Invalid cluster host, partition or account.')
    key=(host,partition,account)
    cached=_CACHE.get(key)
    if cached and time.monotonic()-cached[0]<120:return dict(cached[1])
    command=shlex.join(['env','-u','BASH_ENV','-u','ENV','python3','-','--partition',partition,'--account',account])
    try:
        result=subprocess.run(['ssh',*connection_options(),'-T','-o','BatchMode=yes','-o','ConnectTimeout=10','--',host,command],
                              input=Path(__file__).read_text(),text=True,capture_output=True,timeout=35,check=False)
    except (OSError,subprocess.TimeoutExpired) as error:raise ValueError('Cannot check SLURM time limits: '+str(error)) from error
    if result.returncode:raise ValueError(result.stderr.strip() or 'Cannot check SLURM time limits.')
    try:
        policy=json.loads(result.stdout)
        if not isinstance(policy,dict) or 'limit_seconds' not in policy:raise ValueError('Missing time policy.')
    except (ValueError,TypeError) as error:raise ValueError('Invalid SLURM time-policy response.') from error
    _CACHE[key]=(time.monotonic(),policy)
    return dict(policy)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--partition',required=True)
    parser.add_argument('--account',default='')
    parser.add_argument('--partition-info')
    parser.add_argument('--minutes',action='store_true')
    args=parser.parse_args()
    policy=local_policy(args.partition,args.account,args.partition_info)
    print((policy['limit_seconds']//60 if policy['limit_seconds'] is not None else 0) if args.minutes else json.dumps(policy))


if __name__=='__main__':
    try:main()
    except ValueError as error:raise SystemExit(str(error))
