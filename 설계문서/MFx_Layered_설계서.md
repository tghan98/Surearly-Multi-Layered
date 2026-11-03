# MFx_Layered 설계서

Surearly Multi 펌웨어 — 5계층 구조 (규격 `firmware_layered_coding_v2.0.3.html`)

> **이 문서는 현재 구조의 원본이다.** 구조가 바뀌면 여기를 고친다.
> 날짜별 경과와 결정 근거는 `작업문서\진행사항\YYYY-MM-DD_*.md`에 있다.
> 최초 작성 2026-11-02.

---

## 1. 프로젝트 프로파일

| 항목 | 값 |
|---|---|
| 제품 / 회로도 | Surearly Multi / orcad-surearly_multi-v1.1-r1574 (수정본) |
| MCU / Flash / RAM | STM8L151F3 / 8 KB / 1 KB |
| 툴체인 | IAR EWSTM8 3.10.4 / STM8L15x StdPeriph |
| 핀 설정 원본 | 회로도 + `MFx_New_Ver\Core\main\main.h` |
| 전원 | CR1632 140 mAh → **배터리 프로파일 §13 적용** |
| 클럭 / tick | HSI /8 = 2 MHz, TIM4 **1 ms** |
| 부트로더 | 없음 (벡터 재배치 불필요) |
| 리턴 타입 | `int8_t` — Module 고유 코드 64~127 |
| 슬롯 | 1 ms + 10 ms |
| 워치독 | 미사용 (Halt 중 IWDG 동작 문제) |
| 디버그 출력 | 없음 (통계 getter + 디버거) |
| 기능 참조 원본 | `MFx_New_Ver` (동작만 참조) |
| 공통 골격 참조 | `Easy_Plus\Main\FW\New_FW\User` |

---

## 2. 확정 상수

| 상수 | 값 | 근거 |
|---|---|---|
| TIM4 분주 | 8 | 1 ms당 250칸. 실측 보정 시 세밀도가 분주 16의 2배 |
| TIM4 ARR | **249** | 2,000,000 ÷ 8 ÷ 1000 − 1 (계산값) |
| `Ret_t` | `int8_t` | 8bit MCU, RAM 1 KB (규격 §9) |
| `RET_MODULE_BASE` | 64 | 규격 §9 int8 프로파일 |
| 슬롯 주기 | 1 ms / 10 ms | 규격 §12 기본 |

> **ARR 249는 아직 보정하지 않은 계산값이다.** 실제 fMASTER는 약 1.92 MHz로
> 측정된 바 있다(`MFx_New_Ver` `DM_HW_Drv.c` ARR=149). 보정 전에는 모든 시간이
> 약 4% 길게 흐른다. **시간 의존 시험을 하지 말 것.**
> 보정 방법: 2분 대기(`ERR_RUN_TIME`)를 걸고 실측.

---

## 3. 폴더 구조

```
MFx_Layered\
├─ Src\        main.c, stm8l15x_it.c
├─ Inc\        stm8l15x_it.h, stm8l15x_conf.h
├─ Drivers\    STM8L15x_StdPeriph_Driver\
├─ EWSTM8\     MFx_Layered.ewp / .ewd / .eww
└─ User\
   ├─ 01_HW_Drive\
   ├─ 02_Module\
   ├─ 03_App\     (IDE 그룹: 1_Ctrl / 2_Exec / 3_Data)
   └─ System_Main.c / .h
```

---

## 4. System 계층 (작성 완료)

| 파일 | 공개 함수 |
|---|---|
| `System_Main.c/.h` | `_EarlyInit` / `_Init` / `_Run` / `_SafeStop`<br>`_GetState` / `_GetInitErr` / `_GetResetCause` / `_GetSlotStat` |

한 루프: `s_RunSlot(1ms)` → `s_RunSlot(10ms)` → `s_StateMachine()` → `wfi()`

상태: `SYS_STATE_BOOT` → `SYS_STATE_RUN`, 치명 고장 시 `SYS_STATE_SAFE_STOP`

---

## 5. Driver 계층 (작성 완료 2026-11-02)

규격 §6 "주변장치 1개 = 파일 1개"를 적용했다. 기존 `DM_HW_Drv.c` 한 파일(공개 함수 19개)을
6개로 나눈다.

### 5-1. 파일과 공개 함수

| 파일 | 공개 함수 | 기존 대응 | 신규/재사용 |
|---|---|---|---|
| `HW_Tick_Drive` | `_Init` / `_Get` / `_Elapsed` / `_IsDue` / `_Isr` | `SystemTick_TIM4_Init`, `GetSystemTick`, TIM4 ISR | **완료** |
| `HW_CLK_Drive` | `HW_CLK_Init()` | `SystemClock_Init` | 재사용 |
| `HW_GPIO_Drive` | `HW_GPIO_Init()`<br>`HW_GPIO_Write(id, level)`<br>`HW_GPIO_Read(id)` | `GPIO_Init`, `LED_Control`, LCD 핀 직접 조작 | 재구성 |
| `HW_ADC_Drive` | `HW_ADC_Init()`<br>`HW_ADC_Enable()` / `_Disable()`<br>`HW_ADC_Read(id, p_out)` | `ADC_Init`, `ChannelSelect`, `Read`, `ChannelDeselect` | 재구성 |
| `HW_UART_Drive` | `HW_UART_Init()`<br>`HW_UART_Write(id, buf, len)`<br>`HW_UART_Read(id, buf, max, p_len)`<br>`HW_UART_Isr()` | `USART_Init`, `SendByte`, `SendWord`, `WaitTxComplete` | 재구성 |
| `HW_EEPROM_Drive` | `HW_EEPROM_Read(off, buf, len)`<br>`HW_EEPROM_Write(off, buf, len)` | `EEPROM_Read`, `EEPROM_Write` | 재사용 |
| `HW_PWR_Drive` | `HW_PWR_PrepareHalt()`<br>`HW_PWR_EnterHalt()`<br>`HW_PWR_Resume()`<br>`HW_PWR_SystemReset()` | `Power_PrepareSleep`, `SystemHalt_WakeupOnStick`, `Power_Resume` | 재사용 |

### 5-2. 클럭을 Driver에 둔 이유

세 안(전용 파일 / `HW_PWR_Drive`에 합침 / `System_Main`)을 검토해 **전용 파일**로 정했다.

- 규격 §19, Surearly SMART가 "Driver 초기화 시점: GPIO·**클럭**·tick만 부팅 때"로
  클럭을 Driver로 취급한다. 같은 회사·같은 STM8 제품과 맞춘다.
- 파일 하나가 늘어도 플래시는 거의 늘지 않는다. 클럭 설정은 `CLK_SYSCLKDivConfig` 4 B
  한 줄이고, 어디에 두든 같은 4 B다. (측정 전 "파일이 늘면 플래시도 는다"고 본 것은
  과도한 우려였다.)

**주변장치별 클럭 on/off는 `HW_CLK_Drive`에 모으지 않는다.** 각 Driver가
`CLK_PeripheralClockConfig()`를 직접 부른다. 모으면 ADC·UART가 모두 `HW_CLK_Drive`에
의존해 Driver끼리 얽힌다. Driver는 원래 벤더 라이브러리를 쓰는 계층이라 직접 호출이 맞다.
규격 §13 항목 3("주변장치는 필요할 때 켜고 쓰고 끈다. 클럭까지 Off")도 이 형태를 요구한다.

### 5-3. 논리 ID

핀 번호·채널 번호는 Driver 밖으로 나가지 않는다 (규격 §6).

```c
typedef enum {
    HW_GPIO_LED1 = 0,        /* PB4 */
    HW_GPIO_LED2,            /* PB3 */
    HW_GPIO_LCD_COM,         /* PB2 */
    HW_GPIO_LCD_SEG_BOOK,    /* PC5 */
    HW_GPIO_LCD_SEG_NO,      /* PC6 */
    HW_GPIO_LCD_SEG_YES,     /* PD0 */
    HW_GPIO_LCD_SEG_DROP,    /* PC4 - 회로도는 PB0, 실물은 PC4 */
    HW_GPIO_STICK_SW,        /* PB1 */
    HW_GPIO_COUNT
} HW_GPIO_Id;

typedef enum {
    HW_ADC_FRNT = 0,   /* ADC_Channel_13, Q1 */
    HW_ADC_MIDL,       /* ADC_Channel_12, Q2 */
    HW_ADC_REAR,       /* ADC_Channel_11, Q3 */
    HW_ADC_COUNT
} HW_ADC_Id;
```

ID → 포트·핀 매핑은 `.c` 안 `static const` 테이블(`HW_GPIO_Map`, designated initializer)에 둔다.

ADC 핀(PB5/6/7)과 USART 핀(PA2/PA3)은 `HW_GPIO_Id`에 넣지 않는다. 각각
`HW_ADC_Drive` / `HW_UART_Drive`가 아날로그·대체기능으로 설정한다.

### 5-4. 없어지는 함수

| 기존 | 처리 | 근거 |
|---|---|---|
| `SystemSleep_10ms()` | **삭제** | 규격 §7 "대기 루프·지연 함수 금지". 기다림은 "시작 시각 저장 → 다음 `_Process()`에서 경과 확인"으로 바꾼다 |
| `USART_WaitTxComplete()` | `HW_UART_Write` 내부로 흡수 | 규격 §6 "블로킹 함수는 timeout 인자 필수" |

`SystemSleep_10ms` 제거가 이번 리팩토링의 핵심이다. 상사 지시
("블로킹 구조를 어떻게 하면 좋을지 물어봐")에 대한 답이 이 부분이다.

### 5-5. 극성 처리

LED는 음극 구동이라 High = OFF다. 규격 §12 기본을 따른다.

| 계층 | 아는 것 |
|---|---|
| `HW_GPIO_Drive` | 물리 레벨만 (`HW_GPIO_Write(id, 0)` = 핀을 Low로) |
| Module (Led) | "Low = 켜짐"이라는 의미 변환 |

극성 테이블을 Driver에 두는 안은 쓰지 않는다. Active Low 핀이 LED 2개뿐이라
테이블을 둘 이득이 없다.

### 5-6. 공통 골격 대비 차이 (규격 §15 기록 대상)

| 항목 | 공통 골격 | 이 프로젝트 | 이유 |
|---|---|---|---|
| UART 수신 | 링 버퍼 + 폴링 | 동일 | 기본값 채택 |
| GPIO 극성 | Driver는 물리 레벨 | 동일 | 기본값 채택 |
| 슬롯 실행시간 측정 | 측정 | **미구현** | 플래시 8 KB |
| 벤더 라이브러리 | 수정 안 함 | `stm8l15x.h` typedef 수정 | 타입 충돌 (§7-2) |

---

### 5-7. 기존 FW와 달라진 동작

| 항목 | 기존 (`MFx_New_Ver`) | 새것 | 이유 |
|---|---|---|---|
| UART 수신 | 인터럽트 안에서 `M`/`R`/`Q` 명령을 판별하고 플래그를 세움 (`User_Main.c:141~`) | 인터럽트는 바이트를 링 버퍼(16 B)에 담기만 함. 판별은 Module | 규격 §10 "ISR은 플래그·카운터·링 버퍼 적재만" |
| ADC 시간 보정 | 변환 1회마다 `SystemTick_AddUntracked_us(198)` | **제거** | TIM4가 1 ms로 자유 구동하고 인터럽트가 계속 세므로 잃는 시간이 없다 |
| ADC 타임아웃 | 폴링 횟수 2000회 | tick 기준 2 ms | 규격 §6 "블로킹 함수는 timeout 필수", §10 "ms timeout은 2 이상" |
| EEPROM 범위 초과 | **조용히 건너뜀** — 부른 쪽은 저장된 줄 안다 | `RET_ERR_RANGE` 반환 | 실패가 보여야 한다. **호출부가 반환값을 확인하도록 바뀌어야 함** |
| UART 송신 대기 | `while (TC == RESET);` — 무한 | 바이트당 5 ms 상한 | 규격 §6 |
| MCU 리셋 | 없음 | `HW_PWR_SystemReset()` (WWDG) | 규격 §5 "리셋은 System만, Driver 함수로" |

링 버퍼를 16 B로 잡은 근거: QC 명령은 한 글자(`M`/`R`/`Q`)이고 9600 baud에서 한 바이트가
약 1.04 ms 걸린다. 10 ms 슬롯마다 비우면 최대 10 바이트가 쌓인다. 2의 거듭제곱 조건
(규격 §6)과 여유를 함께 만족하는 가장 작은 값이 16이다.

### 5-8. 인터럽트 연결

`Src\stm8l15x_it.c`에 세 개. 모두 Driver 함수 한 줄만 부른다 (규격 §6).

| 벡터 | 핸들러 | 본문 |
|---|---|---|
| 25 | `TIM4_UPD_OVF_TRG_IRQHandler` | `HW_Tick_Isr();` |
| 9 | `EXTI1_IRQHandler` | `HW_PWR_WakeIsr();` — 스틱 삽입 |
| 28 | `USART1_RX_TIM5_CC_IRQHandler` | `HW_UART_Isr();` |

기존에는 세 개 모두 `User_Main.c`에 있었다.


## 6. Module 계층 (작성 완료 2026-11-02)

부품 1개 = 파일 1개 (규격 §7). 네 개로 나눴다.

| Module | 역할 | 쓰는 Driver | 기존 출처 |
|---|---|---|---|
| `Optic` | LED 점등 → 안정화 → ADC 측정 → 배율 적용 | GPIO, ADC, Tick | `DM_Optic_Handle_App` |
| `Lcd` | 세그먼트 구동, COM 위상 반전, 깜박임 | GPIO, Tick | `DM_LCD_Stick_Check_App` |
| `Eeprom` | 비차단 분할 쓰기, 즉시 읽기 | EEPROM | `DM_Rom_Handl_App` (쓰기 부분) |
| `PcComm` | 명령 프레임 해석, 응답 스트리밍 | UART | `User_Main.c` ISR + `Send_RawDump` |

### 6-1. 스틱 유무 판정은 App에 둔다 (결정)

`DM_Optic_Handle_App`에 있던 `Check_StickPresent*` 세 함수는 Module로 옮기지 않는다.
`Optic`은 측정값(`Optic_GetRaw` / `Optic_GetScaled`)만 돌려주고, 문턱값 비교는 App이 한다.

근거 셋.

1. 문턱값 2,600 / 556은 **사양 상수**다. 규격 §8이 `APP_DATA_Config.h`에 모으라고 한다.
   상수가 App에 있는데 판정만 Module에 있으면 둘이 떨어진다.
2. **판정 방식이 바뀔 예정이다.** 살짝 빠진 스틱은 값이 내려가는 게 아니라 올라간다
   (정상 3,368 / 살짝 빠짐 3,618 / 빈 상태 701). 현재는 단방향 판정이라 못 잡는다.
   양방향 판정이나 채널 간 비교가 들어오면 로직이 커지는데, App에 있으면 `Optic`을
   건드리지 않고 바꾼다.
3. 규격 §7의 경계 — Module은 "센서가 얼마를 읽었다"까지, "그래서 스틱이 있다"는 App.

대안(별도 `Stick` Module)의 근거는 "판정이 세 군데서 필요해 중복된다"였다.
App 안에서 함수 하나로 모으면 해결된다고 보고 채택하지 않았다.

### 6-2. `Optic` — 비차단 측정

```
Optic_Start(ch) → LED ON, ADC ON, 시각 기록
  SETTLE   20 ms 경과 확인          (돌아가며 다른 일 가능)
  SAMPLE   버림 1회 + 21회 변환     (한 패스 안에서)
  LED OFF, ADC OFF
  RECOVER  10 ms 경과 확인
  IDLE
```

| 항목 | 값 | 근거 |
|---|---|---|
| 안정화 | 20 ms | 100 kΩ × 22 nF = 2.2 ms → 약 99.99 % |
| 유효 표본 | 정렬 후 가운데 16개 평균 | 기존과 동일 |
| 정렬 | 삽입 정렬 | 20개 / 2 MHz에서 버블보다 빠름 |
| 회복 | 10 ms | 다음 20 mA 버스트 전 전원 회복 |

**주석 정정**: 기존 코드는 RC 시정수를 4.7 ms로 적어두었다. 22 nF 확정(2026-10-30)에
따라 **2.2 ms**가 맞다.

**공통 골격 대비 차이**: 변환 21회(약 4.2 ms)를 한 패스 안에서 처리해 10 ms 슬롯
예산 2 ms를 넘긴다. 패스로 쪼개면 20 mA LED 점등 시간이 4.2 ms → 약 200 ms로 48배가
된다. 스케줄 초과보다 전지 소모가 크다고 판단했다.

### 6-3. `Lcd` — 시간 기준을 호출 횟수에서 tick으로

| | 기존 | 새것 |
|---|---|---|
| COM 반전 | 불릴 때마다 무조건 | 모듈이 10 ms를 직접 셈 |
| 깜박임 | 호출 50회 세기 | 500 ms 경과 확인 |

기존 방식은 **부르는 주기가 곧 LCD 주파수**였다. 호출 위치가 바뀌면 조용히 틀어진다
— 이중 스로틀링 버그와 같은 종류다.

아이콘이 모두 꺼져 있어도 COM 반전은 계속한다. 멈추면 패널에 직류가 걸린다.

### 6-4. `Eeprom` — 주소 지도는 App이 갖는다

모듈은 "이 바이트들을 이 위치에 써라"만 안다. 어느 주소에 무엇이 있는지는 App 데이터 층.
규격 §8, Easy Plus의 `Eeprom` 모듈과 같은 구조.

| | 기존 | 새것 |
|---|---|---|
| 32바이트 저장 | 한 번에 — 약 200 ms 정지 | 10 ms마다 1바이트, 32회 분할 |
| 쓰기 실패 | 알 수 없음 | `Eeprom_TakeWriteFail()`로 보고 |

**호출 계약**: RAM 1 KB이므로 버퍼를 복사하지 않고 포인터만 보관한다.
`Eeprom_IsBusy()`가 0이 될 때까지 호출자가 그 버퍼를 건드리면 안 된다.

### 6-5. `PcComm` — 해석과 송신을 모두 ISR 밖으로

| | 기존 | 새것 |
|---|---|---|
| 명령 해석 | **인터럽트 안**에서 글자 모으고 즉시 실행 | 인터럽트는 수신만, 해석은 10 ms 슬롯 |
| 덤프 송신 | 약 250 B를 한 번에 — **약 260 ms 정지** | 10 ms마다 4 B 스트리밍 |

프레임 형식은 그대로 유지한다(명령 글자 1개 + CR/LF). QC 프로그램이 의존한다.

`PcComm`은 `M`의 **의미를 모른다.** 글자를 그대로 큐에 넣고, 해석은 App 명령 창구가 한다
(규격 §8).

이를 위해 Driver에 `HW_UART_TryWriteByte()`를 추가했다 — 보낼 수 있으면 보내고 아니면
`RET_BUSY`. 기다리지 않는다.

## 7. App 계층

미착수.

## 8. 공통 골격 대비 차이 (규격 §15)

| 항목 | 공통 골격 | 이 프로젝트 | 이유 |
|---|---|---|---|
| 슬롯 실행시간 측정 | 측정 | **미구현** | 플래시 8 KB. 실행 시간은 스케줄에 필요 없고, 실행 횟수·지연 횟수만 집계. 슬롯이 넘치면 재검토 |
| 슬롯 예산 | 10 ms 슬롯당 2 ms | `Optic` 측정 중 **약 4.2 ms** | 변환 21회를 쪼개면 20 mA LED 점등 시간이 48배가 된다 (§6-2) |
| EEPROM 쓰기 | 비차단 청크 | 동일 (1 B/패스) | 1 B 프로그래밍이 수 ms라 패스당 1 B가 상한 |
| 워치독 | `SYS_USE_WDG` | **미사용** | Halt 중 IWDG가 멈추지 않음 (Surearly SMART와 동일) |
| 벤더 라이브러리 | 수정 안 함 | `stm8l15x.h` typedef 6줄 → `#include <stdint.h>` | `<stdint.h>`와 타입 충돌 (`Pe256` 6건). 규격 §12 허용, §19 Surearly SMART 전례. **라이브러리를 새로 받아 덮어쓰면 재발한다** |
| tick | 1 ms | 1 ms | 동일 |
| UART 수신 | 링 버퍼 + 폴링 | 동일 | 기본값 |
| GPIO 극성 | Driver는 물리 레벨 | 동일 | Active Low 핀이 LED 2개뿐 |

## 9. 플래시 예산

| | 값 |
|---|---|
| 용량 | 8,192 B |
| 기존 펌웨어(`MFx_New_Ver`) 사용량 | 7,436 B |
| 여유 | 756 B |

### 측정 경과

| 시점 | 직접 쓴 코드 | 기존 대응 | 순증 | 겉보기 잔여 |
|---|---|---|---|---|
| 뼈대 (System + Tick) | 343 B | 127 B | +216 B | 540 B |
| Driver 완료 (2026-11-02) | 536 B + 표 32 B | 240 B | **+328 B** | **428 B** |
| Module 완료 (2026-11-02) | 2,229 B | — | — | **3,887 B** |
| App 완료 | 미측정 | | | |

### 428 B를 믿으면 안 된다

Driver 공개 함수 대부분이 **아직 빌드 결과에 없다.** 아무도 부르지 않아 링커가 뺐다.

| 빌드에 들어간 것 | 빠진 것 |
|---|---|
| `HW_Tick_Isr`, `HW_UART_Isr`, `HW_PWR_WakeIsr` (인터럽트에서 불림)<br>`HW_CLK_Init`, `HW_GPIO_Init`, `HW_Tick_Init` (System_Main이 부름) | `HW_GPIO_Write` / `_Read`<br>`HW_ADC_Enable` / `_Disable` / `_Read`<br>`HW_UART_Write` / `_Read` / `_GetStat`<br>`HW_EEPROM_Read` / `_Write`<br>`HW_PWR_PrepareHalt` / `_EnterHalt` / `_Resume` / `_SystemReset` |

Module이 올라오면 빠진 함수들이 전부 들어온다. 대략 **300~500 B를 더 봐야 한다.**

**Module 완료 결과 (2026-11-02): 들어간다.**

| | 값 |
|---|---|
| 현재 사용 (코드 4,114 + 상수 191) | 4,305 B |
| 남은 기능 추정 | 약 2,800 B (`DM_Main_Sq_App` 1,675 + `DM_Rom_Handl_App` 775 + 기타) |
| `HW_PWR` 미반영분 | 약 100~150 B (App이 Halt를 부르면 들어옴) |
| **예상 최종** | **약 7,200 B** |
| 여유 | 약 1,000 B |

기존 펌웨어(7,436 B)보다 작게 나올 전망이다. 벤더 함수가 Driver로 감싸이면서
안 쓰는 것이 빠진 영향이 크다 (`stm8l15x_usart.o` 416 B → 119 B).

### 넘칠 때 쓸 카드 (규격 §12, 기록 조건부)

| 카드 | 예상 효과 |
|---|---|
| 슬롯 통계 끄기 (`SYS_USE_SLOT_STAT = 0`) | 수백 B |
| 컴파일러 최적화를 크기 우선으로 (현재 High/Balanced) | 수백 B |
| 슬롯을 10 ms 하나만 | 수십 B |
