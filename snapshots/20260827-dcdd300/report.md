### filesize8gib / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5293 | 7035 | 7082 | - | 6778 | - | 7068 |
| 2 | 5255 | - | 7074 | 7082 | 7027 | - | 6973 |
| 4 | 5293 | - | - | 7029 | - | 6795 | - |
| 8 | 5286 | - | - | - | - | 7085 | - |

### filesize8gib / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4996 | 6777 | 7075 | - | 7058 | - | 6782 |
| 2 | 5247 | - | 7041 | 7036 | 7025 | - | 6921 |
| 4 | 5283 | - | - | 7034 | - | 7083 | - |
| 8 | 5268 | - | - | - | - | 7062 | - |

### filesize8gib / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5259 | 7022 | 6879 | - | 7054 | - | 7054 |
| 2 | 5290 | - | 6770 | 6794 | 7058 | - | 6913 |
| 4 | 4960 | - | - | 7076 | - | 7048 | - |
| 8 | 5279 | - | - | - | - | 7039 | - |

### imagenetish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | - | 798 | 1285 | 1881 | - | - | 2626 |
| 2 | 970 | - | - | 2386 | - | - | - |
| 4 | 1418 | - | - | - | - | - | - |
| 8 | 1978 | - | - | - | - | - | - |

### imagenetish / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | - | 890 | 1509 | 2342 | - | - | 2773 |
| 2 | 1105 | - | - | 2787 | - | - | - |
| 4 | 1726 | - | - | - | - | - | - |
| 8 | 2452 | - | - | - | - | - | - |

### imagenetish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | - | - | - | - | - | - | 664 |
| 2 | - | - | - | 583 | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### lmcacheish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5036 | 6101 | 5094 | - | 4955 | - | 4893 |
| 2 | 5797 | - | 5004 | 5263 | 5520 | - | 4872 |
| 4 | 5504 | - | - | 5435 | - | 4889 | - |
| 8 | 4997 | - | - | - | - | 4904 | - |

### lmcacheish / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5011 | 6042 | 5244 | - | 4856 | - | 4887 |
| 2 | 5714 | - | 4870 | 5368 | 5061 | - | 4870 |
| 4 | 5469 | - | - | 4871 | - | 4848 | - |
| 8 | 5030 | - | - | - | - | 4841 | - |

### lmcacheish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5040 | 6389 | 6384 | - | 6374 | - | 6396 |
| 2 | 4901 | - | 6152 | 6151 | 6132 | - | 6150 |
| 4 | 4896 | - | - | 6121 | - | 6134 | - |
| 8 | 4899 | - | - | - | - | 6128 | - |

### tiktokish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4904 | 5964 | 6075 | - | 5000 | - | 5036 |
| 2 | 5707 | - | 5047 | 5001 | 5007 | - | 5013 |
| 4 | 5757 | - | - | 5004 | - | 5020 | - |
| 8 | 5280 | - | - | - | - | 5037 | - |

### tiktokish / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4885 | 5890 | 6042 | - | 4953 | - | 4963 |
| 2 | 5641 | - | 5025 | 4957 | 4987 | - | 4994 |
| 4 | 5676 | - | - | 4964 | - | 4988 | - |
| 8 | 5236 | - | - | - | - | 5010 | - |

### tiktokish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4893 | 6213 | 6210 | - | 6212 | - | 6206 |
| 2 | 4665 | - | 5854 | 5869 | 5849 | - | 5870 |
| 4 | 4637 | - | - | 5821 | - | 5851 | - |
| 8 | 4624 | - | - | - | - | 5824 | - |

## Aligned-only (OPENDS_AISIO_ASSUME_ALIGNED_ONLY=1)

Datasets whose files are not LBA-multiples fail by construction and show as `-`.

### filesize8gib / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5191 | 7040 | - | - | - | - | 7092 |
| 2 | - | - | - | 6823 | - | - | - |
| 4 | - | - | - | - | - | 7052 | - |
| 8 | - | - | - | - | - | 6949 | - |

### filesize8gib / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4946 | 7020 | - | - | - | - | 7059 |
| 2 | - | - | - | 7077 | - | - | - |
| 4 | - | - | - | - | - | 7057 | - |
| 8 | - | - | - | - | - | 6899 | - |

### filesize8gib / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5290 | 7025 | - | - | - | - | 7061 |
| 2 | - | - | - | 7054 | - | - | - |
| 4 | - | - | - | - | - | 6951 | - |
| 8 | - | - | - | - | - | 7060 | - |

### lmcacheish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5042 | 6080 | - | - | - | - | 4867 |
| 2 | - | - | - | 5337 | - | - | - |
| 4 | - | - | - | - | - | 4942 | - |
| 8 | - | - | - | - | - | 4858 | - |

### lmcacheish / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5043 | 6400 | - | - | - | - | 6396 |
| 2 | - | - | - | 6401 | - | - | - |
| 4 | - | - | - | - | - | 6395 | - |
| 8 | - | - | - | - | - | 6369 | - |

### lmcacheish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5041 | 6368 | - | - | - | - | 6380 |
| 2 | - | - | - | 6152 | - | - | - |
| 4 | - | - | - | - | - | 6132 | - |
| 8 | - | - | - | - | - | 6128 | - |

## assume_aligned_only=1, idle_spin_us=0

### filesize8gib / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4944 | - | - | - | - | - | - |
| 2 | - | - | - | - | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### filesize8gib / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5183 | - | - | - | - | - | - |
| 2 | - | - | - | - | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### filesize8gib / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5283 | - | - | - | - | - | - |
| 2 | - | - | - | - | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### lmcacheish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5031 | - | - | - | - | - | - |
| 2 | - | - | - | - | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### lmcacheish / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5047 | - | - | - | - | - | - |
| 2 | - | - | - | - | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### lmcacheish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4795 | - | - | - | - | - | - |
| 2 | - | - | - | - | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

## busy_spin=1

### filesize8gib / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5255 | - | 6798 | - | - | - | - |
| 2 | - | - | - | 7053 | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### filesize8gib / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5274 | - | 7064 | - | - | - | - |
| 2 | - | - | - | 6898 | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### filesize8gib / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5313 | - | 6862 | - | - | - | - |
| 2 | - | - | - | 7066 | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### lmcacheish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5041 | - | 5241 | - | - | - | - |
| 2 | - | - | - | 5440 | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### lmcacheish / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5000 | - | 5271 | - | - | - | - |
| 2 | - | - | - | 5405 | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### lmcacheish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5050 | - | 6370 | - | - | - | - |
| 2 | - | - | - | 6396 | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### tiktokish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4908 | - | 6082 | - | - | - | - |
| 2 | - | - | - | 4978 | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### tiktokish / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4880 | - | 6005 | - | - | - | - |
| 2 | - | - | - | 4940 | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### tiktokish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4896 | - | 6215 | - | - | - | - |
| 2 | - | - | - | 6231 | - | - | - |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

## idle_spin_us=0

### filesize8gib / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4948 | - | 7073 | - | - | - | - |
| 2 | 5287 | - | - | 6945 | - | - | 7047 |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### filesize8gib / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4947 | - | 7051 | - | - | - | - |
| 2 | 5273 | - | - | 6879 | - | - | 6901 |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### filesize8gib / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5277 | - | 7098 | - | - | - | - |
| 2 | 5004 | - | - | 7056 | - | - | 7049 |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### lmcacheish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 5043 | - | 5151 | - | - | - | - |
| 2 | 5736 | - | - | 5403 | - | - | 4914 |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### lmcacheish / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4988 | - | 5223 | - | - | - | - |
| 2 | 5717 | - | - | 5388 | - | - | 4866 |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### lmcacheish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4813 | - | 5993 | - | - | - | - |
| 2 | 4907 | - | - | 6141 | - | - | 6134 |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### tiktokish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4897 | - | 6071 | - | - | - | - |
| 2 | 5692 | - | - | 5028 | - | - | 5116 |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### tiktokish / stream (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4867 | - | 6011 | - | - | - | - |
| 2 | 5630 | - | - | 4931 | - | - | 5007 |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

### tiktokish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 512 |
|---|---|---|---|---|---|---|---|
| 1 | 4479 | - | 5540 | - | - | - | - |
| 2 | 4645 | - | - | 5879 | - | - | 5852 |
| 4 | - | - | - | - | - | - | - |
| 8 | - | - | - | - | - | - | - |

## GDS vs OpenDS

### queue_depth=1, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 5293 | - |
| filesize8gib | stream | 2599 | 4996 | 1.92 |
| filesize8gib | sync | 6520 | 5259 | 0.81 |
| lmcacheish | async | - | 5036 | - |
| lmcacheish | stream | 4991 | 5011 | 1.00 |
| lmcacheish | sync | 5533 | 5040 | 0.91 |
| tiktokish | async | - | 4904 | - |
| tiktokish | stream | 5101 | 4885 | 0.96 |
| tiktokish | sync | 4614 | 4893 | 1.06 |

### queue_depth=1, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 5255 | - |
| filesize8gib | stream | 2599 | 5247 | 2.02 |
| filesize8gib | sync | 6520 | 5290 | 0.81 |
| imagenetish | async | - | 970 | - |
| imagenetish | stream | 875 | 1105 | 1.26 |
| lmcacheish | async | - | 5797 | - |
| lmcacheish | stream | 4991 | 5714 | 1.14 |
| lmcacheish | sync | 5533 | 4901 | 0.89 |
| tiktokish | async | - | 5707 | - |
| tiktokish | stream | 5101 | 5641 | 1.11 |
| tiktokish | sync | 4614 | 4665 | 1.01 |

### queue_depth=1, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 5293 | - |
| filesize8gib | stream | 2599 | 5283 | 2.03 |
| filesize8gib | sync | 6520 | 4960 | 0.76 |
| imagenetish | async | - | 1418 | - |
| imagenetish | stream | 875 | 1726 | 1.97 |
| lmcacheish | async | - | 5504 | - |
| lmcacheish | stream | 4991 | 5469 | 1.10 |
| lmcacheish | sync | 5533 | 4896 | 0.88 |
| tiktokish | async | - | 5757 | - |
| tiktokish | stream | 5101 | 5676 | 1.11 |
| tiktokish | sync | 4614 | 4637 | 1.00 |

### queue_depth=1, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 5286 | - |
| filesize8gib | stream | 2599 | 5268 | 2.03 |
| filesize8gib | sync | 6520 | 5279 | 0.81 |
| imagenetish | async | - | 1978 | - |
| imagenetish | stream | 875 | 2452 | 2.80 |
| lmcacheish | async | - | 4997 | - |
| lmcacheish | stream | 4991 | 5030 | 1.01 |
| lmcacheish | sync | 5533 | 4899 | 0.89 |
| tiktokish | async | - | 5280 | - |
| tiktokish | stream | 5101 | 5236 | 1.03 |
| tiktokish | sync | 4614 | 4624 | 1.00 |

### queue_depth=2, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 7035 | - |
| filesize8gib | stream | 2599 | 6777 | 2.61 |
| filesize8gib | sync | 6520 | 7022 | 1.08 |
| imagenetish | async | - | 798 | - |
| imagenetish | stream | 875 | 890 | 1.02 |
| lmcacheish | async | - | 6101 | - |
| lmcacheish | stream | 4991 | 6042 | 1.21 |
| lmcacheish | sync | 5533 | 6389 | 1.15 |
| tiktokish | async | - | 5964 | - |
| tiktokish | stream | 5101 | 5890 | 1.15 |
| tiktokish | sync | 4614 | 6213 | 1.35 |

### queue_depth=4, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 7082 | - |
| filesize8gib | stream | 2599 | 7075 | 2.72 |
| filesize8gib | sync | 6520 | 6879 | 1.06 |
| imagenetish | async | - | 1285 | - |
| imagenetish | stream | 875 | 1509 | 1.72 |
| lmcacheish | async | - | 5094 | - |
| lmcacheish | stream | 4991 | 5244 | 1.05 |
| lmcacheish | sync | 5533 | 6384 | 1.15 |
| tiktokish | async | - | 6075 | - |
| tiktokish | stream | 5101 | 6042 | 1.18 |
| tiktokish | sync | 4614 | 6210 | 1.35 |

### queue_depth=4, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 7074 | - |
| filesize8gib | stream | 2599 | 7041 | 2.71 |
| filesize8gib | sync | 6520 | 6770 | 1.04 |
| lmcacheish | async | - | 5004 | - |
| lmcacheish | stream | 4991 | 4870 | 0.98 |
| lmcacheish | sync | 5533 | 6152 | 1.11 |
| tiktokish | async | - | 5047 | - |
| tiktokish | stream | 5101 | 5025 | 0.99 |
| tiktokish | sync | 4614 | 5854 | 1.27 |

### queue_depth=8, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| imagenetish | async | - | 1881 | - |
| imagenetish | stream | 875 | 2342 | 2.68 |

### queue_depth=8, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 7082 | - |
| filesize8gib | stream | 2599 | 7036 | 2.71 |
| filesize8gib | sync | 6520 | 6794 | 1.04 |
| imagenetish | async | - | 2386 | - |
| imagenetish | stream | 875 | 2787 | 3.19 |
| imagenetish | sync | 343 | 583 | 1.70 |
| lmcacheish | async | - | 5263 | - |
| lmcacheish | stream | 4991 | 5368 | 1.08 |
| lmcacheish | sync | 5533 | 6151 | 1.11 |
| tiktokish | async | - | 5001 | - |
| tiktokish | stream | 5101 | 4957 | 0.97 |
| tiktokish | sync | 4614 | 5869 | 1.27 |

### queue_depth=8, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 7029 | - |
| filesize8gib | stream | 2599 | 7034 | 2.71 |
| filesize8gib | sync | 6520 | 7076 | 1.09 |
| lmcacheish | async | - | 5435 | - |
| lmcacheish | stream | 4991 | 4871 | 0.98 |
| lmcacheish | sync | 5533 | 6121 | 1.11 |
| tiktokish | async | - | 5004 | - |
| tiktokish | stream | 5101 | 4964 | 0.97 |
| tiktokish | sync | 4614 | 5821 | 1.26 |

### queue_depth=16, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 6778 | - |
| filesize8gib | stream | 2599 | 7058 | 2.72 |
| filesize8gib | sync | 6520 | 7054 | 1.08 |
| lmcacheish | async | - | 4955 | - |
| lmcacheish | stream | 4991 | 4856 | 0.97 |
| lmcacheish | sync | 5533 | 6374 | 1.15 |
| tiktokish | async | - | 5000 | - |
| tiktokish | stream | 5101 | 4953 | 0.97 |
| tiktokish | sync | 4614 | 6212 | 1.35 |

### queue_depth=16, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 7027 | - |
| filesize8gib | stream | 2599 | 7025 | 2.70 |
| filesize8gib | sync | 6520 | 7058 | 1.08 |
| lmcacheish | async | - | 5520 | - |
| lmcacheish | stream | 4991 | 5061 | 1.01 |
| lmcacheish | sync | 5533 | 6132 | 1.11 |
| tiktokish | async | - | 5007 | - |
| tiktokish | stream | 5101 | 4987 | 0.98 |
| tiktokish | sync | 4614 | 5849 | 1.27 |

### queue_depth=32, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 6795 | - |
| filesize8gib | stream | 2599 | 7083 | 2.73 |
| filesize8gib | sync | 6520 | 7048 | 1.08 |
| lmcacheish | async | - | 4889 | - |
| lmcacheish | stream | 4991 | 4848 | 0.97 |
| lmcacheish | sync | 5533 | 6134 | 1.11 |
| tiktokish | async | - | 5020 | - |
| tiktokish | stream | 5101 | 4988 | 0.98 |
| tiktokish | sync | 4614 | 5851 | 1.27 |

### queue_depth=32, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 7085 | - |
| filesize8gib | stream | 2599 | 7062 | 2.72 |
| filesize8gib | sync | 6520 | 7039 | 1.08 |
| lmcacheish | async | - | 4904 | - |
| lmcacheish | stream | 4991 | 4841 | 0.97 |
| lmcacheish | sync | 5533 | 6128 | 1.11 |
| tiktokish | async | - | 5037 | - |
| tiktokish | stream | 5101 | 5010 | 0.98 |
| tiktokish | sync | 4614 | 5824 | 1.26 |

### queue_depth=512, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 7068 | - |
| filesize8gib | stream | 2599 | 6782 | 2.61 |
| filesize8gib | sync | 6520 | 7054 | 1.08 |
| imagenetish | async | - | 2626 | - |
| imagenetish | stream | 875 | 2773 | 3.17 |
| imagenetish | sync | 343 | 664 | 1.93 |
| lmcacheish | async | - | 4893 | - |
| lmcacheish | stream | 4991 | 4887 | 0.98 |
| lmcacheish | sync | 5533 | 6396 | 1.16 |
| tiktokish | async | - | 5036 | - |
| tiktokish | stream | 5101 | 4963 | 0.97 |
| tiktokish | sync | 4614 | 6206 | 1.35 |

### queue_depth=512, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | - | 6973 | - |
| filesize8gib | stream | 2599 | 6921 | 2.66 |
| filesize8gib | sync | 6520 | 6913 | 1.06 |
| lmcacheish | async | - | 4872 | - |
| lmcacheish | stream | 4991 | 4870 | 0.98 |
| lmcacheish | sync | 5533 | 6150 | 1.11 |
| tiktokish | async | - | 5013 | - |
| tiktokish | stream | 5101 | 4994 | 0.98 |
| tiktokish | sync | 4614 | 5870 | 1.27 |

