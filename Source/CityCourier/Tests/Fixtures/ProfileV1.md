# Legacy profile fixture

Produced by the actual UE 5.6.1 version-1 `UProfileSaveGame::Encode` implementation at commit `ab687e5`, before adding any history fields. Temporary `CityCourier.CaptureLegacyProfile` automation ran successfully in `Automation-20261018-121552-2cab5767`; its writer was removed after capture to prevent regeneration with the new serializer.

Values: Version 1, BalanceCents 12345, TotalEarnedCents 30000, CompletedCount 15, FailedCount 3, PowerLevel 1, HandlingLevel 2, BatteryLevel 1, MasterVolume 0.3f, QualityLevel 2. No order history field exists in these bytes. This is synthetic test data, not a player's save.
