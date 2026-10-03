#pragma once
#include <driver/i2s.h>
#include "../../common/pr1_rf_audio.hpp"
#ifndef PR1_AUDIO_RF
#define PR1_AUDIO_RF 0
#endif
#if PR1_AUDIO_RF
namespace rf_audio {
#if PR1_RUNTIME_ROLE == PR1_RUNTIME_ROLE_TX
extern const std::uint8_t clip_start[] asm("_binary_data_clip_adpcm_start");
extern const std::uint8_t clip_end[] asm("_binary_data_clip_adpcm_end");
#ifndef PR1_AUDIO_REPEATS
#define PR1_AUDIO_REPEATS 2  // plays twice after TX power-on, then silence (reset TX to replay)
#endif
pr1::audio::ClipSource source(clip_start,clip_end-clip_start,PR1_AUDIO_REPEATS);
bool fill(std::uint32_t now,std::uint8_t* p,std::size_t n) { return source.fill(now,p,n); }
#else
struct Block { std::array<std::uint8_t,100> data{}; };
pr1::runtime::ctrl::Spsc<Block,32> queue;
std::atomic<std::uint32_t> dropped{0}, frames{0}, missing{0}, duplicates{0}, late{0}, invalid{0}, resets{0};
std::atomic<std::uint32_t> errors{0}, short_writes{0}, max_decode_us{0}, startup{0};
std::atomic<bool> ready{false}, failed{false};
PR1_IRAM void receive(const std::uint8_t* p,std::size_t n) {
 if(n!=100) return;
 Block b; for(unsigned i=0;i<100;i++) b.data[i]=p[i];
 if(!queue.push(b)) dropped.fetch_add(1,std::memory_order_relaxed);
}
void task(void*) {
 pinMode(38,OUTPUT); digitalWrite(38,LOW);
 i2s_config_t c{};
 c.mode=static_cast<i2s_mode_t>(I2S_MODE_MASTER|I2S_MODE_TX);
 c.sample_rate=32000; c.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
 c.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT; c.communication_format=I2S_COMM_FORMAT_STAND_I2S;
 c.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1; c.dma_buf_count=8; c.dma_buf_len=160; c.tx_desc_auto_clear=true;
 i2s_pin_config_t p{}; p.mck_io_num=I2S_PIN_NO_CHANGE;
 p.bck_io_num=40; p.ws_io_num=41; p.data_out_num=39; p.data_in_num=I2S_PIN_NO_CHANGE;
 if(i2s_driver_install(I2S_NUM_1,&c,0,nullptr)!=ESP_OK ||
    i2s_set_pin(I2S_NUM_1,&p)!=ESP_OK || i2s_zero_dma_buffer(I2S_NUM_1)!=ESP_OK) {
   failed.store(true); ready.store(true); vTaskDelete(nullptr); return;
 }
 digitalWrite(38,HIGH); ready.store(true);
 static pr1::audio::Jitter jitter;
 static std::int16_t pcm[188], stereo[376];
 for(;;) {
   Block b;
   // At most one queue's worth per cycle: the producer cannot starve I2S.
   for(unsigned i=0;i<32 && queue.pop(&b);i++) jitter.push(b.data.data());
   const auto t=micros(); jitter.render(pcm); const auto dt=micros()-t;
   if(dt>max_decode_us.load()) max_decode_us.store(dt);
   missing.store(jitter.missing); duplicates.store(jitter.duplicates); late.store(jitter.late);
   invalid.store(jitter.invalid); resets.store(jitter.resets); startup.store(jitter.startup);
   for(unsigned i=0;i<188;i++) stereo[i*2]=stereo[i*2+1]=pcm[i];
   std::size_t written=0;
   if(i2s_write(I2S_NUM_1,stereo,sizeof(stereo),&written,pdMS_TO_TICKS(100))!=ESP_OK) ++errors;
   if(written!=sizeof(stereo)) { ++short_writes; vTaskDelay(1); }  // never spin on a failing driver
   frames.fetch_add(written/4);
 }
}
void print() {
 Serial.printf("PR1A frames=%lu missing_blocks=%lu duplicates=%lu late=%lu invalid=%lu resets=%lu queue_dropped=%lu startup_blocks=%lu write_errors=%lu short_writes=%lu max_decode_us=%lu\n",
   (unsigned long)frames.load(),(unsigned long)missing.load(),(unsigned long)duplicates.load(),
   (unsigned long)late.load(),(unsigned long)invalid.load(),(unsigned long)resets.load(),
   (unsigned long)dropped.load(),(unsigned long)startup.load(),(unsigned long)errors.load(),
   (unsigned long)short_writes.load(),(unsigned long)max_decode_us.load());
}
#endif
bool setup(pr1::runtime::FixedLinkRuntime& runtime) {
#if PR1_RUNTIME_ROLE == PR1_RUNTIME_ROLE_TX
 runtime.setPayloadHooks(fill,nullptr); return true;
#else
 runtime.setPayloadHooks(nullptr,receive);
 if(xTaskCreatePinnedToCore(task,"pr1_audio",6144,nullptr,5,nullptr,0)!=pdPASS) return false;
 const auto start=millis(); while(!ready.load() && millis()-start<2000) delay(1);
 return ready.load() && !failed.load();
#endif
}
}
#endif
