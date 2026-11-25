"""Wall-time policy includes account/QOS exceptions and never rounds above a cap."""
import _bootstrap  # noqa: F401
import unittest
from slurm_time_limits import evaluate, seconds


class TimeLimitTests(unittest.TestCase):
    def policy(self, associations='cpuopt|alice|||normal||', qos='normal|||', partition='PartitionName=CPU-amd MaxTime=06:00:00 QoS=N/A'):
        return evaluate(partition,associations,qos,'cpuopt','alice')

    def test_current_cpuopt_account_still_has_partition_limit(self):
        policy=self.policy()
        self.assertEqual(policy['limit_seconds'],21600)
        self.assertTrue(policy['policy_verified'])
        self.assertFalse(policy['partition_time_override'])

    def test_account_qos_can_explicitly_override_partition_time(self):
        policy=self.policy('cpuopt|alice||long|normal,long||','normal|||\nlong|PartitionTimeLimit|1-00:00:00|')
        self.assertEqual(policy['limit_seconds'],86400)
        self.assertTrue(policy['partition_time_override'])
        self.assertTrue(policy['policy_verified'])
        policy=self.policy('cpuopt|alice||long|long||','long|PartitionTimeLimit||')
        self.assertIsNone(policy['limit_seconds'])

    def test_shorter_qos_and_account_limits_and_precedence(self):
        self.assertEqual(self.policy(qos='normal||02:00:00|')['limit_seconds'],7200)
        self.assertEqual(self.policy('cpuopt|alice|||normal|04:00:00|')['limit_seconds'],14400)
        partition='PartitionName=CPU-amd MaxTime=06:00:00 QoS=part'
        self.assertEqual(self.policy(qos='normal||02:00:00|\npart||01:00:00|',partition=partition)['limit_seconds'],3600)
        self.assertEqual(self.policy(qos='normal|OverPartQOS|02:00:00|\npart||01:00:00|',partition=partition)['limit_seconds'],7200)

    def test_unknown_or_ambiguous_default_never_assumes_an_exception(self):
        policy=self.policy('cpuopt|alice|||normal,long||','normal|||\nlong|PartitionTimeLimit|24:00:00|')
        self.assertEqual(policy['limit_seconds'],21600)
        self.assertFalse(policy['partition_time_override'])
        policy=self.policy('', '')
        self.assertEqual(policy['limit_seconds'],21600)
        self.assertFalse(policy['policy_verified'])

    def test_partition_specific_association_and_subminute_rounding(self):
        rows='cpuopt|alice|||normal|04:00:00|\ncpuopt|alice|CPU-amd||normal|02:00:59|'
        self.assertEqual(self.policy(rows)['limit_seconds'],7200)
        with self.assertRaises(ValueError):self.policy(partition='MaxTime=00:00:59')
        with self.assertRaises(ValueError):self.policy(partition='OverSubscribe=NO')

    def test_slurm_duration_formats(self):
        for value,expected in [('360',21600),('06:00:00',21600),('1-02:03:04',93784),('05:30',330),('UNLIMITED',None)]:
            self.assertEqual(seconds(value),expected)
