# Field run notes (2026-10-07)
- 점검 (silent, 10 s): loss 0.775%, flags [] - OK
- 1 m ref (first): loss 0.642%; operator could not hear the clip (likely missed it). Counters cannot show playback.
- 1 m ref retry: loss 0.456%, concealed 0.013%, sound: clean. Prediction met.
- 5 m LOS (first): loss 4.4%, concealed 1.78%, RSSI -79. EXCLUDED by operator: experiment setup was wrong. Raw data kept, not deleted.
- 5 m LOS repeat: loss 0.947%, concealed 0.059%, RSSI -71, sound: clean. Prediction met.
- 5 m body: loss 9.965%, concealed 4.052%, RSSI -82. Prediction (<5% / <2%) NOT met. Sound feeling not reported yet.
- Next (not run): #4 10 m LOS --silent, #5 20 m, #6 40 m, #7 80 m (only if #6 < 5%). Stop at the first distance with loss > 5%.
