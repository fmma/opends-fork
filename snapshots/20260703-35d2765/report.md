### filesize8gib / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 5273 | 6981 | 7026 | 7032 | 6963 | 7053 | 7059 | 7066 | 7043 | 7058 |
| 2 | 5298 | 6991 | 6734 | 7068 | 7043 | 7026 | 6750 | 7030 | 7015 | 6899 |
| 4 | 5274 | 7016 | 6700 | 6955 | 7042 | 7027 | 7033 | 7034 | 6737 | 7036 |
| 8 | 5210 | 7009 | 6759 | 7031 | 6764 | 7039 | 7029 | 7064 | 7022 | 7033 |

### filesize8gib / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 4977 | 6894 | 7060 | 7053 | 7052 | 7053 | 7059 | 7053 | 7056 | 6910 |
| 2 | 5925 | 6887 | 7047 | 6913 | 7044 | 6753 | 7036 | 6997 | 6597 | 6418 |
| 4 | 4781 | 6577 | 4834 | 4879 | 6349 | 6237 | 6132 | 6692 | 6783 | 6678 |
| 8 | 4707 | 4696 | 4697 | 4782 | 6352 | 5805 | 5015 | 4944 | 5013 | 4909 |

### imagenetish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 625 | 895 | 1513 | 2346 | 2777 | 2783 | 2779 | 2784 | 2784 | 2780 |
| 2 | 1114 | 1527 | 2378 | 2797 | 2797 | 2793 | 2798 | 2796 | 2798 | 2792 |
| 4 | 1733 | 2407 | 2806 | 2805 | 2802 | 2805 | 2802 | 2809 | 2806 | 2803 |
| 8 | 2463 | 2792 | 2794 | 2796 | 2800 | 2798 | 2797 | 2799 | 2803 | 2794 |

### imagenetish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 322 | 351 | 352 | 351 | 352 | 353 | 353 | 352 | 351 | 352 |
| 2 | 397 | 443 | 443 | 442 | 442 | 442 | 443 | 443 | 442 | 443 |
| 4 | 398 | 441 | 441 | 441 | 441 | 441 | 441 | 441 | 441 | 441 |
| 8 | 394 | 434 | 434 | 434 | 433 | 434 | 434 | 434 | 435 | 435 |

### lmcacheish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 5081 | 6137 | 5275 | 4917 | 4876 | 4924 | 4999 | 4960 | 4971 | 4924 |
| 2 | 5802 | 5677 | 4968 | 5377 | 5258 | 4889 | 4904 | 4925 | 4936 | 4942 |
| 4 | 5552 | 4937 | 4858 | 4914 | 4894 | 4918 | 4898 | 4885 | 4913 | 4922 |
| 8 | 5086 | 4909 | 4862 | 4904 | 4933 | 4916 | 4878 | 4891 | 4897 | 4888 |

### lmcacheish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 4822 | 6029 | 6025 | 6015 | 6015 | 6011 | 6015 | 6015 | 6009 | 6002 |
| 2 | 5856 | 6211 | 6226 | 6224 | 6228 | 6237 | 6239 | 6240 | 6227 | 6226 |
| 4 | 6090 | 6285 | 6281 | 6279 | 6288 | 6277 | 6282 | 6274 | 6265 | 6279 |
| 8 | 6093 | 6098 | 6091 | 6091 | 6086 | 6102 | 6094 | 6084 | 6083 | 6084 |

### tiktokish / async (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 4409 | 5071 | 5661 | 5293 | 5329 | 5254 | 5486 | 5497 | 5162 | 5258 |
| 2 | 4998 | 4715 | 5341 | 5073 | 5302 | 5526 | 5372 | 5443 | 5305 | 5464 |
| 4 | 5289 | 5475 | 5319 | 5742 | 5184 | 5459 | 5352 | 5102 | 5301 | 5520 |
| 8 | 4883 | 5200 | 5309 | 5035 | 5217 | 5333 | 5373 | 5322 | 5609 | 5212 |

### tiktokish / sync (MiB/s)

| io_threads \ queue_depth | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 4048 | 4907 | 4993 | 4962 | 5029 | 4840 | 4888 | 4982 | 4849 | 4971 |
| 2 | 4918 | 5097 | 4899 | 4931 | 5046 | 5027 | 4964 | 5069 | 4979 | 5066 |
| 4 | 4916 | 5157 | 5030 | 4991 | 4948 | 5045 | 5156 | 5066 | 5254 | 5189 |
| 8 | 4902 | 5031 | 5070 | 4991 | 4990 | 4961 | 4934 | 5036 | 4921 | 4847 |

## GDS vs OpenDS

### queue_depth=1, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 5273 | 2.17 |
| filesize8gib | sync | 6967 | 4977 | 0.71 |
| imagenetish | async | 868 | 625 | 0.72 |
| imagenetish | sync | 335 | 322 | 0.96 |
| lmcacheish | async | 4961 | 5081 | 1.02 |
| lmcacheish | sync | 5317 | 4822 | 0.91 |
| tiktokish | async | 2515 | 4409 | 1.75 |
| tiktokish | sync | 3899 | 4048 | 1.04 |

### queue_depth=1, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 5298 | 2.18 |
| filesize8gib | sync | 6967 | 5925 | 0.85 |
| imagenetish | async | 868 | 1114 | 1.28 |
| imagenetish | sync | 335 | 397 | 1.19 |
| lmcacheish | async | 4961 | 5802 | 1.17 |
| lmcacheish | sync | 5317 | 5856 | 1.10 |
| tiktokish | async | 2515 | 4998 | 1.99 |
| tiktokish | sync | 3899 | 4918 | 1.26 |

### queue_depth=1, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 5274 | 2.17 |
| filesize8gib | sync | 6967 | 4781 | 0.69 |
| imagenetish | async | 868 | 1733 | 2.00 |
| imagenetish | sync | 335 | 398 | 1.19 |
| lmcacheish | async | 4961 | 5552 | 1.12 |
| lmcacheish | sync | 5317 | 6090 | 1.15 |
| tiktokish | async | 2515 | 5289 | 2.10 |
| tiktokish | sync | 3899 | 4916 | 1.26 |

### queue_depth=1, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 5210 | 2.15 |
| filesize8gib | sync | 6967 | 4707 | 0.68 |
| imagenetish | async | 868 | 2463 | 2.84 |
| imagenetish | sync | 335 | 394 | 1.18 |
| lmcacheish | async | 4961 | 5086 | 1.03 |
| lmcacheish | sync | 5317 | 6093 | 1.15 |
| tiktokish | async | 2515 | 4883 | 1.94 |
| tiktokish | sync | 3899 | 4902 | 1.26 |

### queue_depth=2, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6981 | 2.88 |
| filesize8gib | sync | 6967 | 6894 | 0.99 |
| imagenetish | async | 868 | 895 | 1.03 |
| imagenetish | sync | 335 | 351 | 1.05 |
| lmcacheish | async | 4961 | 6137 | 1.24 |
| lmcacheish | sync | 5317 | 6029 | 1.13 |
| tiktokish | async | 2515 | 5071 | 2.02 |
| tiktokish | sync | 3899 | 4907 | 1.26 |

### queue_depth=2, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6991 | 2.88 |
| filesize8gib | sync | 6967 | 6887 | 0.99 |
| imagenetish | async | 868 | 1527 | 1.76 |
| imagenetish | sync | 335 | 443 | 1.32 |
| lmcacheish | async | 4961 | 5677 | 1.14 |
| lmcacheish | sync | 5317 | 6211 | 1.17 |
| tiktokish | async | 2515 | 4715 | 1.87 |
| tiktokish | sync | 3899 | 5097 | 1.31 |

### queue_depth=2, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7016 | 2.89 |
| filesize8gib | sync | 6967 | 6577 | 0.94 |
| imagenetish | async | 868 | 2407 | 2.77 |
| imagenetish | sync | 335 | 441 | 1.31 |
| lmcacheish | async | 4961 | 4937 | 1.00 |
| lmcacheish | sync | 5317 | 6285 | 1.18 |
| tiktokish | async | 2515 | 5475 | 2.18 |
| tiktokish | sync | 3899 | 5157 | 1.32 |

### queue_depth=2, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7009 | 2.89 |
| filesize8gib | sync | 6967 | 4696 | 0.67 |
| imagenetish | async | 868 | 2792 | 3.22 |
| imagenetish | sync | 335 | 434 | 1.30 |
| lmcacheish | async | 4961 | 4909 | 0.99 |
| lmcacheish | sync | 5317 | 6098 | 1.15 |
| tiktokish | async | 2515 | 5200 | 2.07 |
| tiktokish | sync | 3899 | 5031 | 1.29 |

### queue_depth=4, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7026 | 2.90 |
| filesize8gib | sync | 6967 | 7060 | 1.01 |
| imagenetish | async | 868 | 1513 | 1.74 |
| imagenetish | sync | 335 | 352 | 1.05 |
| lmcacheish | async | 4961 | 5275 | 1.06 |
| lmcacheish | sync | 5317 | 6025 | 1.13 |
| tiktokish | async | 2515 | 5661 | 2.25 |
| tiktokish | sync | 3899 | 4993 | 1.28 |

### queue_depth=4, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6734 | 2.78 |
| filesize8gib | sync | 6967 | 7047 | 1.01 |
| imagenetish | async | 868 | 2378 | 2.74 |
| imagenetish | sync | 335 | 443 | 1.32 |
| lmcacheish | async | 4961 | 4968 | 1.00 |
| lmcacheish | sync | 5317 | 6226 | 1.17 |
| tiktokish | async | 2515 | 5341 | 2.12 |
| tiktokish | sync | 3899 | 4899 | 1.26 |

### queue_depth=4, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6700 | 2.76 |
| filesize8gib | sync | 6967 | 4834 | 0.69 |
| imagenetish | async | 868 | 2806 | 3.23 |
| imagenetish | sync | 335 | 441 | 1.32 |
| lmcacheish | async | 4961 | 4858 | 0.98 |
| lmcacheish | sync | 5317 | 6281 | 1.18 |
| tiktokish | async | 2515 | 5319 | 2.12 |
| tiktokish | sync | 3899 | 5030 | 1.29 |

### queue_depth=4, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6759 | 2.79 |
| filesize8gib | sync | 6967 | 4697 | 0.67 |
| imagenetish | async | 868 | 2794 | 3.22 |
| imagenetish | sync | 335 | 434 | 1.29 |
| lmcacheish | async | 4961 | 4862 | 0.98 |
| lmcacheish | sync | 5317 | 6091 | 1.15 |
| tiktokish | async | 2515 | 5309 | 2.11 |
| tiktokish | sync | 3899 | 5070 | 1.30 |

### queue_depth=8, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7032 | 2.90 |
| filesize8gib | sync | 6967 | 7053 | 1.01 |
| imagenetish | async | 868 | 2346 | 2.70 |
| imagenetish | sync | 335 | 351 | 1.05 |
| lmcacheish | async | 4961 | 4917 | 0.99 |
| lmcacheish | sync | 5317 | 6015 | 1.13 |
| tiktokish | async | 2515 | 5293 | 2.10 |
| tiktokish | sync | 3899 | 4962 | 1.27 |

### queue_depth=8, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7068 | 2.91 |
| filesize8gib | sync | 6967 | 6913 | 0.99 |
| imagenetish | async | 868 | 2797 | 3.22 |
| imagenetish | sync | 335 | 442 | 1.32 |
| lmcacheish | async | 4961 | 5377 | 1.08 |
| lmcacheish | sync | 5317 | 6224 | 1.17 |
| tiktokish | async | 2515 | 5073 | 2.02 |
| tiktokish | sync | 3899 | 4931 | 1.26 |

### queue_depth=8, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6955 | 2.87 |
| filesize8gib | sync | 6967 | 4879 | 0.70 |
| imagenetish | async | 868 | 2805 | 3.23 |
| imagenetish | sync | 335 | 441 | 1.31 |
| lmcacheish | async | 4961 | 4914 | 0.99 |
| lmcacheish | sync | 5317 | 6279 | 1.18 |
| tiktokish | async | 2515 | 5742 | 2.28 |
| tiktokish | sync | 3899 | 4991 | 1.28 |

### queue_depth=8, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7031 | 2.90 |
| filesize8gib | sync | 6967 | 4782 | 0.69 |
| imagenetish | async | 868 | 2796 | 3.22 |
| imagenetish | sync | 335 | 434 | 1.29 |
| lmcacheish | async | 4961 | 4904 | 0.99 |
| lmcacheish | sync | 5317 | 6091 | 1.15 |
| tiktokish | async | 2515 | 5035 | 2.00 |
| tiktokish | sync | 3899 | 4991 | 1.28 |

### queue_depth=16, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6963 | 2.87 |
| filesize8gib | sync | 6967 | 7052 | 1.01 |
| imagenetish | async | 868 | 2777 | 3.20 |
| imagenetish | sync | 335 | 352 | 1.05 |
| lmcacheish | async | 4961 | 4876 | 0.98 |
| lmcacheish | sync | 5317 | 6015 | 1.13 |
| tiktokish | async | 2515 | 5329 | 2.12 |
| tiktokish | sync | 3899 | 5029 | 1.29 |

### queue_depth=16, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7043 | 2.90 |
| filesize8gib | sync | 6967 | 7044 | 1.01 |
| imagenetish | async | 868 | 2797 | 3.22 |
| imagenetish | sync | 335 | 442 | 1.32 |
| lmcacheish | async | 4961 | 5258 | 1.06 |
| lmcacheish | sync | 5317 | 6228 | 1.17 |
| tiktokish | async | 2515 | 5302 | 2.11 |
| tiktokish | sync | 3899 | 5046 | 1.29 |

### queue_depth=16, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7042 | 2.90 |
| filesize8gib | sync | 6967 | 6349 | 0.91 |
| imagenetish | async | 868 | 2802 | 3.23 |
| imagenetish | sync | 335 | 441 | 1.31 |
| lmcacheish | async | 4961 | 4894 | 0.99 |
| lmcacheish | sync | 5317 | 6288 | 1.18 |
| tiktokish | async | 2515 | 5184 | 2.06 |
| tiktokish | sync | 3899 | 4948 | 1.27 |

### queue_depth=16, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6764 | 2.79 |
| filesize8gib | sync | 6967 | 6352 | 0.91 |
| imagenetish | async | 868 | 2800 | 3.23 |
| imagenetish | sync | 335 | 433 | 1.29 |
| lmcacheish | async | 4961 | 4933 | 0.99 |
| lmcacheish | sync | 5317 | 6086 | 1.14 |
| tiktokish | async | 2515 | 5217 | 2.07 |
| tiktokish | sync | 3899 | 4990 | 1.28 |

### queue_depth=32, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7053 | 2.91 |
| filesize8gib | sync | 6967 | 7053 | 1.01 |
| imagenetish | async | 868 | 2783 | 3.21 |
| imagenetish | sync | 335 | 353 | 1.05 |
| lmcacheish | async | 4961 | 4924 | 0.99 |
| lmcacheish | sync | 5317 | 6011 | 1.13 |
| tiktokish | async | 2515 | 5254 | 2.09 |
| tiktokish | sync | 3899 | 4840 | 1.24 |

### queue_depth=32, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7026 | 2.90 |
| filesize8gib | sync | 6967 | 6753 | 0.97 |
| imagenetish | async | 868 | 2793 | 3.22 |
| imagenetish | sync | 335 | 442 | 1.32 |
| lmcacheish | async | 4961 | 4889 | 0.99 |
| lmcacheish | sync | 5317 | 6237 | 1.17 |
| tiktokish | async | 2515 | 5526 | 2.20 |
| tiktokish | sync | 3899 | 5027 | 1.29 |

### queue_depth=32, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7027 | 2.90 |
| filesize8gib | sync | 6967 | 6237 | 0.90 |
| imagenetish | async | 868 | 2805 | 3.23 |
| imagenetish | sync | 335 | 441 | 1.32 |
| lmcacheish | async | 4961 | 4918 | 0.99 |
| lmcacheish | sync | 5317 | 6277 | 1.18 |
| tiktokish | async | 2515 | 5459 | 2.17 |
| tiktokish | sync | 3899 | 5045 | 1.29 |

### queue_depth=32, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7039 | 2.90 |
| filesize8gib | sync | 6967 | 5805 | 0.83 |
| imagenetish | async | 868 | 2798 | 3.23 |
| imagenetish | sync | 335 | 434 | 1.30 |
| lmcacheish | async | 4961 | 4916 | 0.99 |
| lmcacheish | sync | 5317 | 6102 | 1.15 |
| tiktokish | async | 2515 | 5333 | 2.12 |
| tiktokish | sync | 3899 | 4961 | 1.27 |

### queue_depth=64, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7059 | 2.91 |
| filesize8gib | sync | 6967 | 7059 | 1.01 |
| imagenetish | async | 868 | 2779 | 3.20 |
| imagenetish | sync | 335 | 353 | 1.05 |
| lmcacheish | async | 4961 | 4999 | 1.01 |
| lmcacheish | sync | 5317 | 6015 | 1.13 |
| tiktokish | async | 2515 | 5486 | 2.18 |
| tiktokish | sync | 3899 | 4888 | 1.25 |

### queue_depth=64, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6750 | 2.78 |
| filesize8gib | sync | 6967 | 7036 | 1.01 |
| imagenetish | async | 868 | 2798 | 3.23 |
| imagenetish | sync | 335 | 443 | 1.32 |
| lmcacheish | async | 4961 | 4904 | 0.99 |
| lmcacheish | sync | 5317 | 6239 | 1.17 |
| tiktokish | async | 2515 | 5372 | 2.14 |
| tiktokish | sync | 3899 | 4964 | 1.27 |

### queue_depth=64, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7033 | 2.90 |
| filesize8gib | sync | 6967 | 6132 | 0.88 |
| imagenetish | async | 868 | 2802 | 3.23 |
| imagenetish | sync | 335 | 441 | 1.32 |
| lmcacheish | async | 4961 | 4898 | 0.99 |
| lmcacheish | sync | 5317 | 6282 | 1.18 |
| tiktokish | async | 2515 | 5352 | 2.13 |
| tiktokish | sync | 3899 | 5156 | 1.32 |

### queue_depth=64, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7029 | 2.90 |
| filesize8gib | sync | 6967 | 5015 | 0.72 |
| imagenetish | async | 868 | 2797 | 3.22 |
| imagenetish | sync | 335 | 434 | 1.29 |
| lmcacheish | async | 4961 | 4878 | 0.98 |
| lmcacheish | sync | 5317 | 6094 | 1.15 |
| tiktokish | async | 2515 | 5373 | 2.14 |
| tiktokish | sync | 3899 | 4934 | 1.27 |

### queue_depth=128, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7066 | 2.91 |
| filesize8gib | sync | 6967 | 7053 | 1.01 |
| imagenetish | async | 868 | 2784 | 3.21 |
| imagenetish | sync | 335 | 352 | 1.05 |
| lmcacheish | async | 4961 | 4960 | 1.00 |
| lmcacheish | sync | 5317 | 6015 | 1.13 |
| tiktokish | async | 2515 | 5497 | 2.19 |
| tiktokish | sync | 3899 | 4982 | 1.28 |

### queue_depth=128, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7030 | 2.90 |
| filesize8gib | sync | 6967 | 6997 | 1.00 |
| imagenetish | async | 868 | 2796 | 3.22 |
| imagenetish | sync | 335 | 443 | 1.32 |
| lmcacheish | async | 4961 | 4925 | 0.99 |
| lmcacheish | sync | 5317 | 6240 | 1.17 |
| tiktokish | async | 2515 | 5443 | 2.16 |
| tiktokish | sync | 3899 | 5069 | 1.30 |

### queue_depth=128, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7034 | 2.90 |
| filesize8gib | sync | 6967 | 6692 | 0.96 |
| imagenetish | async | 868 | 2809 | 3.24 |
| imagenetish | sync | 335 | 441 | 1.32 |
| lmcacheish | async | 4961 | 4885 | 0.98 |
| lmcacheish | sync | 5317 | 6274 | 1.18 |
| tiktokish | async | 2515 | 5102 | 2.03 |
| tiktokish | sync | 3899 | 5066 | 1.30 |

### queue_depth=128, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7064 | 2.91 |
| filesize8gib | sync | 6967 | 4944 | 0.71 |
| imagenetish | async | 868 | 2799 | 3.23 |
| imagenetish | sync | 335 | 434 | 1.30 |
| lmcacheish | async | 4961 | 4891 | 0.99 |
| lmcacheish | sync | 5317 | 6084 | 1.14 |
| tiktokish | async | 2515 | 5322 | 2.12 |
| tiktokish | sync | 3899 | 5036 | 1.29 |

### queue_depth=256, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7043 | 2.90 |
| filesize8gib | sync | 6967 | 7056 | 1.01 |
| imagenetish | async | 868 | 2784 | 3.21 |
| imagenetish | sync | 335 | 351 | 1.05 |
| lmcacheish | async | 4961 | 4971 | 1.00 |
| lmcacheish | sync | 5317 | 6009 | 1.13 |
| tiktokish | async | 2515 | 5162 | 2.05 |
| tiktokish | sync | 3899 | 4849 | 1.24 |

### queue_depth=256, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7015 | 2.89 |
| filesize8gib | sync | 6967 | 6597 | 0.95 |
| imagenetish | async | 868 | 2798 | 3.22 |
| imagenetish | sync | 335 | 442 | 1.32 |
| lmcacheish | async | 4961 | 4936 | 0.99 |
| lmcacheish | sync | 5317 | 6227 | 1.17 |
| tiktokish | async | 2515 | 5305 | 2.11 |
| tiktokish | sync | 3899 | 4979 | 1.28 |

### queue_depth=256, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6737 | 2.78 |
| filesize8gib | sync | 6967 | 6783 | 0.97 |
| imagenetish | async | 868 | 2806 | 3.23 |
| imagenetish | sync | 335 | 441 | 1.32 |
| lmcacheish | async | 4961 | 4913 | 0.99 |
| lmcacheish | sync | 5317 | 6265 | 1.18 |
| tiktokish | async | 2515 | 5301 | 2.11 |
| tiktokish | sync | 3899 | 5254 | 1.35 |

### queue_depth=256, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7022 | 2.89 |
| filesize8gib | sync | 6967 | 5013 | 0.72 |
| imagenetish | async | 868 | 2803 | 3.23 |
| imagenetish | sync | 335 | 435 | 1.30 |
| lmcacheish | async | 4961 | 4897 | 0.99 |
| lmcacheish | sync | 5317 | 6083 | 1.14 |
| tiktokish | async | 2515 | 5609 | 2.23 |
| tiktokish | sync | 3899 | 4921 | 1.26 |

### queue_depth=512, io_threads=1

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7058 | 2.91 |
| filesize8gib | sync | 6967 | 6910 | 0.99 |
| imagenetish | async | 868 | 2780 | 3.20 |
| imagenetish | sync | 335 | 352 | 1.05 |
| lmcacheish | async | 4961 | 4924 | 0.99 |
| lmcacheish | sync | 5317 | 6002 | 1.13 |
| tiktokish | async | 2515 | 5258 | 2.09 |
| tiktokish | sync | 3899 | 4971 | 1.28 |

### queue_depth=512, io_threads=2

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 6899 | 2.84 |
| filesize8gib | sync | 6967 | 6418 | 0.92 |
| imagenetish | async | 868 | 2792 | 3.22 |
| imagenetish | sync | 335 | 443 | 1.32 |
| lmcacheish | async | 4961 | 4942 | 1.00 |
| lmcacheish | sync | 5317 | 6226 | 1.17 |
| tiktokish | async | 2515 | 5464 | 2.17 |
| tiktokish | sync | 3899 | 5066 | 1.30 |

### queue_depth=512, io_threads=4

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7036 | 2.90 |
| filesize8gib | sync | 6967 | 6678 | 0.96 |
| imagenetish | async | 868 | 2803 | 3.23 |
| imagenetish | sync | 335 | 441 | 1.32 |
| lmcacheish | async | 4961 | 4922 | 0.99 |
| lmcacheish | sync | 5317 | 6279 | 1.18 |
| tiktokish | async | 2515 | 5520 | 2.19 |
| tiktokish | sync | 3899 | 5189 | 1.33 |

### queue_depth=512, io_threads=8

| Dataset | Mode | GDS (MiB/s) | OpenDS (MiB/s) | Speedup |
|---|---|---|---|---|
| filesize8gib | async | 2426 | 7033 | 2.90 |
| filesize8gib | sync | 6967 | 4909 | 0.70 |
| imagenetish | async | 868 | 2794 | 3.22 |
| imagenetish | sync | 335 | 435 | 1.30 |
| lmcacheish | async | 4961 | 4888 | 0.99 |
| lmcacheish | sync | 5317 | 6084 | 1.14 |
| tiktokish | async | 2515 | 5212 | 2.07 |
| tiktokish | sync | 3899 | 4847 | 1.24 |

