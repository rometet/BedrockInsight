from test_analytics_companion import Fixture, BASE


class NotificationTests(Fixture):
    def test_notifications_not_success_kinds_or_unknown_causes(self):
        self.player()
        self.event("player_death")
        self.event("item_use")
        self.event("player_death_notification",cause="unknown")
        self.event("item_use_attempt",cause="unknown")
        self.assertIsNone(self.report()["players"][0]["death_notifications"])
        self.assertIsNone(self.report()["players"][0]["item_use_attempts"])
        self.event("player_death_notification",cause="ll26.60.player_die_notification.v1")
        self.event("item_use_attempt",cause="ll26.60.item_use_attempt.v1")
        r=self.report()["players"][0]
        self.assertEqual(r["death_notifications"],1)
        self.assertEqual(r["item_use_attempts"],1)

    def test_absence_is_unknown_and_rankings_positive_only(self):
        self.player()
        self.player("other","Other")
        self.event("player_death_notification",cause="ll26.60.player_die_notification.v1")
        r=self.report(metric="death_notifications")
        self.assertEqual(len(r["ranking"]),1)
        self.assertEqual(r["ranking"][0]["value"],1)
        self.assertIsNone(r["players"][1]["death_notifications"])
        self.assertIsNone(r["totals"]["item_use_attempts"])

    def test_jst_period_without_record_is_not_asserted_zero_coverage(self):
        self.player()
        self.event("item_use_attempt",at=BASE-1,cause="ll26.60.item_use_attempt.v1")
        self.assertIsNone(self.report(period="day",date="2026-11-06")["players"][0]["item_use_attempts"])
        self.assertEqual(self.report(period="day",date="2026-11-05")["players"][0]["item_use_attempts"],1)
        self.assertEqual(self.report(period="all")["totals"]["item_use_attempts"],1)
