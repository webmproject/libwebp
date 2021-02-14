// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Image transform methods for lossless encoder.
//
// Authors: Vikas Arora (vikaas.arora@gmail.com)
//          Jyrki Alakuijala (jyrki@google.com)
//          Urvang Joshi (urvang@google.com)

#include "src/dsp/dsp.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "src/dec/vp8li_dec.h"
#include "src/utils/endian_inl_utils.h"
#include "src/dsp/lossless.h"
#include "src/dsp/lossless_common.h"
#include "src/dsp/yuv.h"

// lookup table for small values of log2(int)
const float kLog2Table[LOG_LOOKUP_IDX_MAX] = {
  0.0000000000000000f, 0.0000000000000000f,
  1.0000000000000000f, 1.5849625007211560f,
  2.0000000000000000f, 2.3219280948873621f,
  2.5849625007211560f, 2.8073549220576041f,
  3.0000000000000000f, 3.1699250014423121f,
  3.3219280948873621f, 3.4594316186372973f,
  3.5849625007211560f, 3.7004397181410921f,
  3.8073549220576041f, 3.9068905956085187f,
  4.0000000000000000f, 4.0874628412503390f,
  4.1699250014423121f, 4.2479275134435852f,
  4.3219280948873626f, 4.3923174227787606f,
  4.4594316186372973f, 4.5235619560570130f,
  4.5849625007211560f, 4.6438561897747243f,
  4.7004397181410917f, 4.7548875021634682f,
  4.8073549220576037f, 4.8579809951275718f,
  4.9068905956085187f, 4.9541963103868749f,
  5.0000000000000000f, 5.0443941193584533f,
  5.0874628412503390f, 5.1292830169449663f,
  5.1699250014423121f, 5.2094533656289501f,
  5.2479275134435852f, 5.2854022188622487f,
  5.3219280948873626f, 5.3575520046180837f,
  5.3923174227787606f, 5.4262647547020979f,
  5.4594316186372973f, 5.4918530963296747f,
  5.5235619560570130f, 5.5545888516776376f,
  5.5849625007211560f, 5.6147098441152083f,
  5.6438561897747243f, 5.6724253419714951f,
  5.7004397181410917f, 5.7279204545631987f,
  5.7548875021634682f, 5.7813597135246599f,
  5.8073549220576037f, 5.8328900141647412f,
  5.8579809951275718f, 5.8826430493618415f,
  5.9068905956085187f, 5.9307373375628866f,
  5.9541963103868749f, 5.9772799234999167f,
  6.0000000000000000f, 6.0223678130284543f,
  6.0443941193584533f, 6.0660891904577720f,
  6.0874628412503390f, 6.1085244567781691f,
  6.1292830169449663f, 6.1497471195046822f,
  6.1699250014423121f, 6.1898245588800175f,
  6.2094533656289501f, 6.2288186904958804f,
  6.2479275134435852f, 6.2667865406949010f,
  6.2854022188622487f, 6.3037807481771030f,
  6.3219280948873626f, 6.3398500028846243f,
  6.3575520046180837f, 6.3750394313469245f,
  6.3923174227787606f, 6.4093909361377017f,
  6.4262647547020979f, 6.4429434958487279f,
  6.4594316186372973f, 6.4757334309663976f,
  6.4918530963296747f, 6.5077946401986963f,
  6.5235619560570130f, 6.5391588111080309f,
  6.5545888516776376f, 6.5698556083309478f,
  6.5849625007211560f, 6.5999128421871278f,
  6.6147098441152083f, 6.6293566200796094f,
  6.6438561897747243f, 6.6582114827517946f,
  6.6724253419714951f, 6.6865005271832185f,
  6.7004397181410917f, 6.7142455176661224f,
  6.7279204545631987f, 6.7414669864011464f,
  6.7548875021634682f, 6.7681843247769259f,
  6.7813597135246599f, 6.7944158663501061f,
  6.8073549220576037f, 6.8201789624151878f,
  6.8328900141647412f, 6.8454900509443747f,
  6.8579809951275718f, 6.8703647195834047f,
  6.8826430493618415f, 6.8948177633079437f,
  6.9068905956085187f, 6.9188632372745946f,
  6.9307373375628866f, 6.9425145053392398f,
  6.9541963103868749f, 6.9657842846620869f,
  6.9772799234999167f, 6.9886846867721654f,
  7.0000000000000000f, 7.0112272554232539f,
  7.0223678130284543f, 7.0334230015374501f,
  7.0443941193584533f, 7.0552824355011898f,
  7.0660891904577720f, 7.0768155970508308f,
  7.0874628412503390f, 7.0980320829605263f,
  7.1085244567781691f, 7.1189410727235076f,
  7.1292830169449663f, 7.1395513523987936f,
  7.1497471195046822f, 7.1598713367783890f,
  7.1699250014423121f, 7.1799090900149344f,
  7.1898245588800175f, 7.1996723448363644f,
  7.2094533656289501f, 7.2191685204621611f,
  7.2288186904958804f, 7.2384047393250785f,
  7.2479275134435852f, 7.2573878426926521f,
  7.2667865406949010f, 7.2761244052742375f,
  7.2854022188622487f, 7.2946207488916270f,
  7.3037807481771030f, 7.3128829552843557f,
  7.3219280948873626f, 7.3309168781146167f,
  7.3398500028846243f, 7.3487281542310771f,
  7.3575520046180837f, 7.3663222142458160f,
  7.3750394313469245f, 7.3837042924740519f,
  7.3923174227787606f, 7.4008794362821843f,
  7.4093909361377017f, 7.4178525148858982f,
  7.4262647547020979f, 7.4346282276367245f,
  7.4429434958487279f, 7.4512111118323289f,
  7.4594316186372973f, 7.4676055500829976f,
  7.4757334309663976f, 7.4838157772642563f,
  7.4918530963296747f, 7.4998458870832056f,
  7.5077946401986963f, 7.5156998382840427f,
  7.5235619560570130f, 7.5313814605163118f,
  7.5391588111080309f, 7.5468944598876364f,
  7.5545888516776376f, 7.5622424242210728f,
  7.5698556083309478f, 7.5774288280357486f,
  7.5849625007211560f, 7.5924570372680806f,
  7.5999128421871278f, 7.6073303137496104f,
  7.6147098441152083f, 7.6220518194563764f,
  7.6293566200796094f, 7.6366246205436487f,
  7.6438561897747243f, 7.6510516911789281f,
  7.6582114827517946f, 7.6653359171851764f,
  7.6724253419714951f, 7.6794800995054464f,
  7.6865005271832185f, 7.6934869574993252f,
  7.7004397181410917f, 7.7073591320808825f,
  7.7142455176661224f, 7.7210991887071855f,
  7.7279204545631987f, 7.7347096202258383f,
  7.7414669864011464f, 7.7481928495894605f,
  7.7548875021634682f, 7.7615512324444795f,
  7.7681843247769259f, 7.7747870596011736f,
  7.7813597135246599f, 7.7879025593914317f,
  7.7944158663501061f, 7.8008998999203047f,
  7.8073549220576037f, 7.8137811912170374f,
  7.8201789624151878f, 7.8265484872909150f,
  7.8328900141647412f, 7.8392037880969436f,
  7.8454900509443747f, 7.8517490414160571f,
  7.8579809951275718f, 7.8641861446542797f,
  7.8703647195834047f, 7.8765169465649993f,
  7.8826430493618415f, 7.8887432488982591f,
  7.8948177633079437f, 7.9008668079807486f,
  7.9068905956085187f, 7.9128893362299619f,
  7.9188632372745946f, 7.9248125036057812f,
  7.9307373375628866f, 7.9366379390025709f,
  7.9425145053392398f, 7.9483672315846778f,
  7.9541963103868749f, 7.9600019320680805f,
  7.9657842846620869f, 7.9715435539507719f,
  7.9772799234999167f, 7.9829935746943103f,
  7.9886846867721654f, 7.9943534368588577f
};

// x * (1. - log2(x))
const float kSLog2m1Table[LOG_LOOKUP_IDX_MAX] = {
     0.000000000000000f,     1.0000000000000000f,    0.0000000000000000f,
    -1.7548875021634682f,   -4.0000000000000000f,   -6.6096404744368105f,
    -9.5097750043269365f,  -12.6514844544032297f,  -16.0000000000000000f,
   -19.5293250129808094f,  -23.2192809488736209f,  -27.0537478050102713f,
   -31.0195500086538729f,  -35.1057163358341953f,  -39.3029689088064558f,
   -43.6033589341277832f,  -48.0000000000000000f,  -52.4868683012557682f,
   -57.0586500259616187f,  -61.7106227554281190f,  -66.4385618977472490f,
   -71.2386658783539701f,  -76.1074956100205355f,  -81.0419249893112976f,
   -86.0391000173077458f,  -91.0964047443681153f,  -96.2114326716683905f,
  -101.3819625584136475f, -106.6059378176128973f, -111.8814488586995850f,
  -117.2067178682555664f, -122.5800856219931205f, -128.0000000000000000f,
  -133.4650059388289662f, -138.9737366025115364f, -144.5249055930738109f,
  -150.1173000519232232f, -155.7497745282711605f, -161.4212455108562381f,
  -167.1306865356276887f, -172.8771237954944979f, -178.6596321893414370f,
  -184.4773317567079403f, -190.3293844521901974f, -196.2149912200410711f,
  -202.1333893348353570f, -208.0838499786225952f, -214.0656760288489693f,
  -220.0782000346154916f, -226.1207823616452117f, -232.1928094887362306f,
  -238.2936924405462662f, -244.4228653433367811f, -250.5797840918495467f,
  -256.7639251168272949f, -262.9747842438562770f, -269.2118756352257947f,
  -275.4747308073902445f, -281.7628977173991416f, -288.0759399123486446f,
  -294.4134357365111327f, -300.7749775913360963f, -307.1601712439862695f,
  -313.5686351804947662f, -320.0000000000000000f, -326.4539078468495177f,
  -332.9300118776579325f, -339.4279757606707335f, -345.9474732050230728f,
  -352.4881875176936887f, -359.0498111861476218f, -365.6320454848324175f,
  -372.2346001038464465f, -378.8571927982412717f, -385.4995490565423211f,
  -392.1614017871910391f, -398.8424910217124761f, -405.5425636335073705f,
  -412.2613730712553775f, -418.9986791059911297f, -425.7542475909889959f,
  -432.5278502336545898f, -439.3192643786828739f, -446.1282728017947079f,
  -452.9546635134158805f, -459.7982295717046668f, -466.6587689043803948f,
  -473.5360841388393283f, -480.4299824400821421f, -487.3402753560093856f,
  -494.2667786696707140f, -501.2093122580813542f, -508.1676999572451905f,
  -515.1417694330468748f, -522.1313520576978817f, -529.1362827914400668f,
  -536.1564000692310401f, -543.1915456921514078f, -550.2415647232903666f,
  -557.3063053878813662f, -564.3856189774724044f, -571.4793597579312063f,
  -578.5873848810924756f, -585.7095542998714564f, -592.8457306866735053f,
  -599.9957793549428970f, -607.1595681836990934f, -614.3369675449227998f,
  -621.5278502336545898f, -628.7320914006849080f, -635.9495684877125541f,
  -643.1801611648618291f, -650.4237512704515893f, -657.6802227529162792f,
  -664.9494616147804891f, -672.2313558586031377f, -679.5257954347982832f,
  -686.8326721912583253f, -694.1518798246972892f, -701.4833138336452976f,
  -708.8268714730222655f, -716.1824517102259051f, -723.5499551826721927f,
  -730.9292841567264531f, -738.3203424879725389f, -745.7230355827608719f,
  -753.1372703609895325f, -760.5629552200649641f, -768.0000000000000000f,
  -775.4483159495997597f, -782.9078156936990354f, -790.3784132014059196f,
  -797.8600237553158649f, -805.3525639216582022f, -812.8559515213414670f,
  -820.3701056018621784f, -827.8949464100461455f, -835.4303953655920623f,
  -842.9763750353873775f, -850.5328091085675624f, -858.0996223722952436f,
  -865.6767406882298701f, -873.2640909696648350f, -880.8616011593096573f,
  -888.4692002076928929f, -896.0868180521655404f, -903.7143855964825434f,
  -911.3518346909455659f, -918.9990981130846421f, -926.6561095488619912f,
  -934.3228035743820783f, -941.9991156380868915f, -949.6849820434249523f,
  -957.3803399319757546f, -965.0851272670147409f, -972.7992828175067643f,
  -980.5227461425107549f, -988.2554575759854743f, -995.9973582119822595f,
  -1003.7483898902125929f, -1011.5084951819779917f, -1019.2776173764533496f,
  -1027.0557004673091797f, -1034.8426891396654810f, -1042.6385287573657479f,
  -1050.4431653505596387f, -1058.2565456035895295f, -1066.0786168431666283f,
  -1073.9093270268317610f, -1081.7486247316892332f, -1089.5964591434092199f,
  -1097.4527800454886801f, -1105.3175378087607896f, -1113.1906833811533488f,
  -1121.0721682776786565f, -1128.9619445706575789f, -1136.8599648801643980f,
  -1144.7661823646906214f, -1152.6805507120188850f, -1160.6030241303019466f,
  -1168.5335573393415416f, -1176.4721055620602783f, -1184.4186245161627085f,
  -1192.3730704059798882f, -1200.3353999144903810f, -1208.3055701955177028f,
  -1216.2835388660937497f, -1224.2692639989879808f, -1232.2627041153957634f,
  -1240.2638181777826958f, -1248.2725655828801337f, -1256.2889061548280552f,
  -1264.3128001384620802f, -1272.3442081927396430f, -1280.3830913843028156f,
  -1288.4294111811741459f, -1296.4831294465807332f, -1304.5442084329060890f,
  -1312.6126107757627324f, -1320.6882994881862032f, -1328.7712379549448087f,
  -1336.8613899269646481f, -1344.9587195158624127f, -1353.0631911885907357f,
  -1361.1747697621849511f, -1369.2934203986164903f, -1377.4191085997429127f,
  -1385.5518002023602548f, -1393.6914613733470105f, -1401.8380586049045178f,
  -1409.9915587098857941f, -1418.1519288172160032f, -1426.3191363673981868f,
  -1434.4931491081035801f, -1442.6739350898455996f, -1450.8614626617340946f,
  -1459.0557004673091797f, -1467.2566174404521462f, -1475.4641828013698159f,
  -1483.6783660526571111f, -1491.8991369754251082f, -1500.1264656255063983f,
  -1508.3603223297236582f, -1516.6006776822280244f, -1524.8475025409031787f,
  -1533.1007680238333251f, -1541.3604455058325584f, -1549.6265066150376697f,
  -1557.8989232295609781f, -1566.1776674742000068f, -1574.4627117172062754f,
  -1582.7540285671091169f, -1591.0515908695965663f, -1599.3553717044474070f,
  -1607.6653443825166505f, -1615.9814824427749045f, -1624.3037596493945784f,
  -1632.6321499888874769f, -1640.9666276672905951f, -1649.3071671073989819f,
  -1657.6537429460445310f, -1666.0063300314209300f, -1674.3649034204518102f,
  -1682.7294383762048255f, -1691.0999103653443854f, -1699.4762950556298620f,
  -1707.8585683134529063f, -1716.2467062014154635f, -1724.6406849759450779f,
  -1733.0404810849520345f, -1741.4460711655217438f, -1749.8574320416437331f,
  -1758.2745407219790650f, -1766.6973743976604965f, -1775.1259104401299282f,
  -1783.5601263990088228f,
};

const VP8LPrefixCode kPrefixEncodeCode[PREFIX_LOOKUP_IDX_MAX] = {
  { 0, 0}, { 0, 0}, { 1, 0}, { 2, 0}, { 3, 0}, { 4, 1}, { 4, 1}, { 5, 1},
  { 5, 1}, { 6, 2}, { 6, 2}, { 6, 2}, { 6, 2}, { 7, 2}, { 7, 2}, { 7, 2},
  { 7, 2}, { 8, 3}, { 8, 3}, { 8, 3}, { 8, 3}, { 8, 3}, { 8, 3}, { 8, 3},
  { 8, 3}, { 9, 3}, { 9, 3}, { 9, 3}, { 9, 3}, { 9, 3}, { 9, 3}, { 9, 3},
  { 9, 3}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4},
  {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4},
  {10, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4},
  {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4},
  {11, 4}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5},
  {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5},
  {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5},
  {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5},
  {12, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5},
  {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5},
  {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5},
  {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5}, {13, 5},
  {13, 5}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6}, {14, 6},
  {14, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6}, {15, 6},
  {15, 6}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7}, {16, 7},
  {16, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
  {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
};

const uint8_t kPrefixEncodeExtraBitsValue[PREFIX_LOOKUP_IDX_MAX] = {
   0,  0,  0,  0,  0,  0,  1,  0,  1,  0,  1,  2,  3,  0,  1,  2,  3,
   0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  4,  5,  6,  7,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
  64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
  96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
  127,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
  64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
  96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126
};

//------------------------------------------------------------------------------
// The threshold till approximate version of log_2 can be used.
#define APPROX_LOG_WITH_CORRECTION_MAX  65536
#define LOG_2_RECIPROCAL 1.44269504088896338700465094007086

float VP8LFastSLog2m1Slow(uint32_t v) {
  assert(v >= LOG_LOOKUP_IDX_MAX);
  if (v < APPROX_LOG_WITH_CORRECTION_MAX) {
    // If we write v as 2^K * A + B, with A >= 1, B < 2^K and K such that we can
    // do a quick lookup to get log2(2^K * A). Then, since A = v >> K and
    //  B = v % 2^K, we have: log2(v) = K + kLog2Table[A] + log2[1 + B/(2^K.A)],
    // and the last term can be approximated to log2[1 + B/(2^K.A)] ~= B/v/log2.
    const uint32_t K = BitsLog2Floor(v) - 7u;
    const float correction = LOG_2_RECIPROCAL * (v & ((1 << K) - 1));
    return -(float)v * (kLog2Table[v >> K] + K - 1) - correction;
  } else {
    return (float)(v - v * log2f(v));
  }
}

//------------------------------------------------------------------------------
// Methods to calculate Entropy (Shannon).

// Compute the combined Shanon's entropy for distribution {X} and {X+Y}
static float CombinedShannonEntropy_C(const int X[256], const int Y[256]) {
  double retval = 0.;
  uint32_t i;
  // This critical function is extremely memory-bond! Any change in code should
  // be tested on various compilers.
  for (i = 0; i < 256; ++i) {
    const uint32_t x = X[i];
    if (x != 0) {
      retval += VP8LFastSLog2m1(x + Y[i]);
      retval += VP8LFastSLog2m1(x);
    } else if (Y[i] != 0) {
      retval += VP8LFastSLog2m1(Y[i]);
    }
  }
  return (float)retval;
}

void VP8LBitEntropyInit(VP8LBitEntropy* const entropy) {
  entropy->entropy = 0.;
  entropy->sum = 0;
  entropy->nonzeros = 0;
  entropy->max_val = 0;
  entropy->nonzero_code = VP8L_NON_TRIVIAL_SYM;
}

void VP8LBitsEntropyUnrefined(const uint32_t* const array, int n,
                              VP8LBitEntropy* const entropy) {
  int i;

  VP8LBitEntropyInit(entropy);

  for (i = 0; i < n; ++i) {
    if (array[i] != 0) {
      entropy->sum += array[i];
      entropy->nonzero_code = i;
      ++entropy->nonzeros;
      entropy->entropy += VP8LFastSLog2m1(array[i]);
      if (entropy->max_val < array[i]) {
        entropy->max_val = array[i];
      }
    }
  }
}

static WEBP_INLINE void GetEntropyUnrefinedHelper(
    uint32_t val, int i, uint32_t* const val_prev, int* const i_prev,
    VP8LBitEntropy* const bit_entropy, VP8LStreaks* const stats) {
  const int streak = i - *i_prev;

  // Gather info for the bit entropy.
  if (*val_prev != 0) {
    bit_entropy->sum += (*val_prev) * streak;
    bit_entropy->nonzeros += streak;
    bit_entropy->nonzero_code = *i_prev;
    bit_entropy->entropy += VP8LFastSLog2m1(*val_prev) * streak;
    if (bit_entropy->max_val < *val_prev) {
      bit_entropy->max_val = *val_prev;
    }
  }
  // Gather info for the Huffman cost.
  stats->counts[*val_prev != 0] += (streak > 3);
  stats->streaks[*val_prev != 0][(streak > 3)] += streak;

  *val_prev = val;
  *i_prev = i;
}

static void GetEntropyUnrefined_C(const uint32_t X[], int length,
                                  VP8LBitEntropy* const bit_entropy,
                                  VP8LStreaks* const stats) {
  int i;
  int i_prev = 0;
  uint32_t x_prev = X[0];

  memset(stats, 0, sizeof(*stats));
  VP8LBitEntropyInit(bit_entropy);

  for (i = 1; i < length; ++i) {
    const uint32_t x = X[i];
    if (x != x_prev) {
      GetEntropyUnrefinedHelper(x, i, &x_prev, &i_prev, bit_entropy, stats);
    }
  }
  GetEntropyUnrefinedHelper(0, i, &x_prev, &i_prev, bit_entropy, stats);
  bit_entropy->entropy -= VP8LFastSLog2m1(bit_entropy->sum);
}

static void GetCombinedEntropyUnrefined_C(const uint32_t X[],
                                          const uint32_t Y[],
                                          int length,
                                          VP8LBitEntropy* const bit_entropy,
                                          VP8LStreaks* const stats) {
  int i = 1;
  int i_prev = 0;
  uint32_t xy_prev = X[0] + Y[0];

  memset(stats, 0, sizeof(*stats));
  VP8LBitEntropyInit(bit_entropy);

  for (i = 1; i < length; ++i) {
    const uint32_t xy = X[i] + Y[i];
    if (xy != xy_prev) {
      GetEntropyUnrefinedHelper(xy, i, &xy_prev, &i_prev, bit_entropy, stats);
    }
  }
  GetEntropyUnrefinedHelper(0, i, &xy_prev, &i_prev, bit_entropy, stats);
  bit_entropy->entropy -= VP8LFastSLog2m1(bit_entropy->sum);
}

//------------------------------------------------------------------------------

void VP8LSubtractGreenFromBlueAndRed_C(uint32_t* argb_data, int num_pixels) {
  int i;
  for (i = 0; i < num_pixels; ++i) {
    const int argb = argb_data[i];
    const int green = (argb >> 8) & 0xff;
    const uint32_t new_r = (((argb >> 16) & 0xff) - green) & 0xff;
    const uint32_t new_b = (((argb >>  0) & 0xff) - green) & 0xff;
    argb_data[i] = (argb & 0xff00ff00u) | (new_r << 16) | new_b;
  }
}

static WEBP_INLINE int ColorTransformDelta(int8_t color_pred, int8_t color) {
  return ((int)color_pred * color) >> 5;
}

static WEBP_INLINE int8_t U32ToS8(uint32_t v) {
  return (int8_t)(v & 0xff);
}

void VP8LTransformColor_C(const VP8LMultipliers* const m, uint32_t* data,
                          int num_pixels) {
  int i;
  for (i = 0; i < num_pixels; ++i) {
    const uint32_t argb = data[i];
    const int8_t green = U32ToS8(argb >>  8);
    const int8_t red   = U32ToS8(argb >> 16);
    int new_red = red & 0xff;
    int new_blue = argb & 0xff;
    new_red -= ColorTransformDelta(m->green_to_red_, green);
    new_red &= 0xff;
    new_blue -= ColorTransformDelta(m->green_to_blue_, green);
    new_blue -= ColorTransformDelta(m->red_to_blue_, red);
    new_blue &= 0xff;
    data[i] = (argb & 0xff00ff00u) | (new_red << 16) | (new_blue);
  }
}

static WEBP_INLINE uint8_t TransformColorRed(uint8_t green_to_red,
                                             uint32_t argb) {
  const int8_t green = U32ToS8(argb >> 8);
  int new_red = argb >> 16;
  new_red -= ColorTransformDelta(green_to_red, green);
  return (new_red & 0xff);
}

static WEBP_INLINE uint8_t TransformColorBlue(uint8_t green_to_blue,
                                              uint8_t red_to_blue,
                                              uint32_t argb) {
  const int8_t green = U32ToS8(argb >>  8);
  const int8_t red   = U32ToS8(argb >> 16);
  uint8_t new_blue = argb & 0xff;
  new_blue -= ColorTransformDelta(green_to_blue, green);
  new_blue -= ColorTransformDelta(red_to_blue, red);
  return (new_blue & 0xff);
}

void VP8LCollectColorRedTransforms_C(const uint32_t* argb, int stride,
                                     int tile_width, int tile_height,
                                     int green_to_red, int histo[]) {
  while (tile_height-- > 0) {
    int x;
    for (x = 0; x < tile_width; ++x) {
      ++histo[TransformColorRed((uint8_t)green_to_red, argb[x])];
    }
    argb += stride;
  }
}

void VP8LCollectColorBlueTransforms_C(const uint32_t* argb, int stride,
                                      int tile_width, int tile_height,
                                      int green_to_blue, int red_to_blue,
                                      int histo[]) {
  while (tile_height-- > 0) {
    int x;
    for (x = 0; x < tile_width; ++x) {
      ++histo[TransformColorBlue((uint8_t)green_to_blue, (uint8_t)red_to_blue,
                                 argb[x])];
    }
    argb += stride;
  }
}

//------------------------------------------------------------------------------

static int VectorMismatch_C(const uint32_t* const array1,
                            const uint32_t* const array2, int length) {
  int match_len = 0;

  while (match_len < length && array1[match_len] == array2[match_len]) {
    ++match_len;
  }
  return match_len;
}

// Bundles multiple (1, 2, 4 or 8) pixels into a single pixel.
void VP8LBundleColorMap_C(const uint8_t* const row, int width, int xbits,
                          uint32_t* dst) {
  int x;
  if (xbits > 0) {
    const int bit_depth = 1 << (3 - xbits);
    const int mask = (1 << xbits) - 1;
    uint32_t code = 0xff000000;
    for (x = 0; x < width; ++x) {
      const int xsub = x & mask;
      if (xsub == 0) {
        code = 0xff000000;
      }
      code |= row[x] << (8 + bit_depth * xsub);
      dst[x >> xbits] = code;
    }
  } else {
    for (x = 0; x < width; ++x) dst[x] = 0xff000000 | (row[x] << 8);
  }
}

//------------------------------------------------------------------------------

static double ExtraCost_C(const uint32_t* population, int length) {
  int i;
  double cost = 0.;
  for (i = 2; i < length - 2; ++i) cost += (i >> 1) * population[i + 2];
  return cost;
}

static double ExtraCostCombined_C(const uint32_t* X, const uint32_t* Y,
                                  int length) {
  int i;
  double cost = 0.;
  for (i = 2; i < length - 2; ++i) {
    const int xy = X[i + 2] + Y[i + 2];
    cost += (i >> 1) * xy;
  }
  return cost;
}

//------------------------------------------------------------------------------

static void AddVector_C(const uint32_t* a, const uint32_t* b, uint32_t* out,
                        int size) {
  int i;
  for (i = 0; i < size; ++i) out[i] = a[i] + b[i];
}

static void AddVectorEq_C(const uint32_t* a, uint32_t* out, int size) {
  int i;
  for (i = 0; i < size; ++i) out[i] += a[i];
}

#define ADD(X, ARG, LEN) do {                                                  \
  if (a->is_used_[X]) {                                                        \
    if (b->is_used_[X]) {                                                      \
      VP8LAddVector(a->ARG, b->ARG, out->ARG, (LEN));                          \
    } else {                                                                   \
      memcpy(&out->ARG[0], &a->ARG[0], (LEN) * sizeof(out->ARG[0]));           \
    }                                                                          \
  } else if (b->is_used_[X]) {                                                 \
    memcpy(&out->ARG[0], &b->ARG[0], (LEN) * sizeof(out->ARG[0]));             \
  } else {                                                                     \
    memset(&out->ARG[0], 0, (LEN) * sizeof(out->ARG[0]));                      \
  }                                                                            \
} while (0)

#define ADD_EQ(X, ARG, LEN) do {                                               \
  if (a->is_used_[X]) {                                                        \
    if (out->is_used_[X]) {                                                    \
      VP8LAddVectorEq(a->ARG, out->ARG, (LEN));                                \
    } else {                                                                   \
      memcpy(&out->ARG[0], &a->ARG[0], (LEN) * sizeof(out->ARG[0]));           \
    }                                                                          \
  }                                                                            \
} while (0)

void VP8LHistogramAdd(const VP8LHistogram* const a,
                      const VP8LHistogram* const b, VP8LHistogram* const out) {
  int i;
  const int literal_size = VP8LHistogramNumCodes(a->palette_code_bits_);
  assert(a->palette_code_bits_ == b->palette_code_bits_);

  if (b != out) {
    ADD(0, literal_, literal_size);
    ADD(1, red_, NUM_LITERAL_CODES);
    ADD(2, blue_, NUM_LITERAL_CODES);
    ADD(3, alpha_, NUM_LITERAL_CODES);
    ADD(4, distance_, NUM_DISTANCE_CODES);
    for (i = 0; i < 5; ++i) {
      out->is_used_[i] = (a->is_used_[i] | b->is_used_[i]);
    }
  } else {
    ADD_EQ(0, literal_, literal_size);
    ADD_EQ(1, red_, NUM_LITERAL_CODES);
    ADD_EQ(2, blue_, NUM_LITERAL_CODES);
    ADD_EQ(3, alpha_, NUM_LITERAL_CODES);
    ADD_EQ(4, distance_, NUM_DISTANCE_CODES);
    for (i = 0; i < 5; ++i) out->is_used_[i] |= a->is_used_[i];
  }
}
#undef ADD
#undef ADD_EQ

//------------------------------------------------------------------------------
// Image transforms.

static void PredictorSub0_C(const uint32_t* in, const uint32_t* upper,
                            int num_pixels, uint32_t* out) {
  int i;
  for (i = 0; i < num_pixels; ++i) out[i] = VP8LSubPixels(in[i], ARGB_BLACK);
  (void)upper;
}

static void PredictorSub1_C(const uint32_t* in, const uint32_t* upper,
                            int num_pixels, uint32_t* out) {
  int i;
  for (i = 0; i < num_pixels; ++i) out[i] = VP8LSubPixels(in[i], in[i - 1]);
  (void)upper;
}

// It subtracts the prediction from the input pixel and stores the residual
// in the output pixel.
#define GENERATE_PREDICTOR_SUB(PREDICTOR_I)                                \
static void PredictorSub##PREDICTOR_I##_C(const uint32_t* in,              \
                                          const uint32_t* upper,           \
                                          int num_pixels, uint32_t* out) { \
  int x;                                                                   \
  assert(upper != NULL);                                                   \
  for (x = 0; x < num_pixels; ++x) {                                       \
    const uint32_t pred =                                                  \
        VP8LPredictor##PREDICTOR_I##_C(in[x - 1], upper + x);              \
    out[x] = VP8LSubPixels(in[x], pred);                                   \
  }                                                                        \
}

GENERATE_PREDICTOR_SUB(2)
GENERATE_PREDICTOR_SUB(3)
GENERATE_PREDICTOR_SUB(4)
GENERATE_PREDICTOR_SUB(5)
GENERATE_PREDICTOR_SUB(6)
GENERATE_PREDICTOR_SUB(7)
GENERATE_PREDICTOR_SUB(8)
GENERATE_PREDICTOR_SUB(9)
GENERATE_PREDICTOR_SUB(10)
GENERATE_PREDICTOR_SUB(11)
GENERATE_PREDICTOR_SUB(12)
GENERATE_PREDICTOR_SUB(13)

//------------------------------------------------------------------------------

VP8LProcessEncBlueAndRedFunc VP8LSubtractGreenFromBlueAndRed;

VP8LTransformColorFunc VP8LTransformColor;

VP8LCollectColorBlueTransformsFunc VP8LCollectColorBlueTransforms;
VP8LCollectColorRedTransformsFunc VP8LCollectColorRedTransforms;

VP8LCostFunc VP8LExtraCost;
VP8LCostCombinedFunc VP8LExtraCostCombined;
VP8LCombinedShannonEntropyFunc VP8LCombinedShannonEntropy;

VP8LGetEntropyUnrefinedFunc VP8LGetEntropyUnrefined;
VP8LGetCombinedEntropyUnrefinedFunc VP8LGetCombinedEntropyUnrefined;

VP8LAddVectorFunc VP8LAddVector;
VP8LAddVectorEqFunc VP8LAddVectorEq;

VP8LVectorMismatchFunc VP8LVectorMismatch;
VP8LBundleColorMapFunc VP8LBundleColorMap;

VP8LPredictorAddSubFunc VP8LPredictorsSub[16];
VP8LPredictorAddSubFunc VP8LPredictorsSub_C[16];

extern void VP8LEncDspInitSSE2(void);
extern void VP8LEncDspInitSSE41(void);
extern void VP8LEncDspInitNEON(void);
extern void VP8LEncDspInitMIPS32(void);
extern void VP8LEncDspInitMIPSdspR2(void);
extern void VP8LEncDspInitMSA(void);

WEBP_DSP_INIT_FUNC(VP8LEncDspInit) {
  VP8LDspInit();

#if !WEBP_NEON_OMIT_C_CODE
  VP8LSubtractGreenFromBlueAndRed = VP8LSubtractGreenFromBlueAndRed_C;

  VP8LTransformColor = VP8LTransformColor_C;
#endif

  VP8LCollectColorBlueTransforms = VP8LCollectColorBlueTransforms_C;
  VP8LCollectColorRedTransforms = VP8LCollectColorRedTransforms_C;

  VP8LExtraCost = ExtraCost_C;
  VP8LExtraCostCombined = ExtraCostCombined_C;
  VP8LCombinedShannonEntropy = CombinedShannonEntropy_C;

  VP8LGetEntropyUnrefined = GetEntropyUnrefined_C;
  VP8LGetCombinedEntropyUnrefined = GetCombinedEntropyUnrefined_C;

  VP8LAddVector = AddVector_C;
  VP8LAddVectorEq = AddVectorEq_C;

  VP8LVectorMismatch = VectorMismatch_C;
  VP8LBundleColorMap = VP8LBundleColorMap_C;

  VP8LPredictorsSub[0] = PredictorSub0_C;
  VP8LPredictorsSub[1] = PredictorSub1_C;
  VP8LPredictorsSub[2] = PredictorSub2_C;
  VP8LPredictorsSub[3] = PredictorSub3_C;
  VP8LPredictorsSub[4] = PredictorSub4_C;
  VP8LPredictorsSub[5] = PredictorSub5_C;
  VP8LPredictorsSub[6] = PredictorSub6_C;
  VP8LPredictorsSub[7] = PredictorSub7_C;
  VP8LPredictorsSub[8] = PredictorSub8_C;
  VP8LPredictorsSub[9] = PredictorSub9_C;
  VP8LPredictorsSub[10] = PredictorSub10_C;
  VP8LPredictorsSub[11] = PredictorSub11_C;
  VP8LPredictorsSub[12] = PredictorSub12_C;
  VP8LPredictorsSub[13] = PredictorSub13_C;
  VP8LPredictorsSub[14] = PredictorSub0_C;  // <- padding security sentinels
  VP8LPredictorsSub[15] = PredictorSub0_C;

  VP8LPredictorsSub_C[0] = PredictorSub0_C;
  VP8LPredictorsSub_C[1] = PredictorSub1_C;
  VP8LPredictorsSub_C[2] = PredictorSub2_C;
  VP8LPredictorsSub_C[3] = PredictorSub3_C;
  VP8LPredictorsSub_C[4] = PredictorSub4_C;
  VP8LPredictorsSub_C[5] = PredictorSub5_C;
  VP8LPredictorsSub_C[6] = PredictorSub6_C;
  VP8LPredictorsSub_C[7] = PredictorSub7_C;
  VP8LPredictorsSub_C[8] = PredictorSub8_C;
  VP8LPredictorsSub_C[9] = PredictorSub9_C;
  VP8LPredictorsSub_C[10] = PredictorSub10_C;
  VP8LPredictorsSub_C[11] = PredictorSub11_C;
  VP8LPredictorsSub_C[12] = PredictorSub12_C;
  VP8LPredictorsSub_C[13] = PredictorSub13_C;
  VP8LPredictorsSub_C[14] = PredictorSub0_C;  // <- padding security sentinels
  VP8LPredictorsSub_C[15] = PredictorSub0_C;

  // If defined, use CPUInfo() to overwrite some pointers with faster versions.
  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_USE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      VP8LEncDspInitSSE2();
#if defined(WEBP_USE_SSE41)
      if (VP8GetCPUInfo(kSSE4_1)) {
        VP8LEncDspInitSSE41();
      }
#endif
    }
#endif
#if defined(WEBP_USE_MIPS32)
    if (VP8GetCPUInfo(kMIPS32)) {
      VP8LEncDspInitMIPS32();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      VP8LEncDspInitMIPSdspR2();
    }
#endif
#if defined(WEBP_USE_MSA)
    if (VP8GetCPUInfo(kMSA)) {
      VP8LEncDspInitMSA();
    }
#endif
  }

#if defined(WEBP_USE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    VP8LEncDspInitNEON();
  }
#endif

  assert(VP8LSubtractGreenFromBlueAndRed != NULL);
  assert(VP8LTransformColor != NULL);
  assert(VP8LCollectColorBlueTransforms != NULL);
  assert(VP8LCollectColorRedTransforms != NULL);
  assert(VP8LExtraCost != NULL);
  assert(VP8LExtraCostCombined != NULL);
  assert(VP8LCombinedShannonEntropy != NULL);
  assert(VP8LGetEntropyUnrefined != NULL);
  assert(VP8LGetCombinedEntropyUnrefined != NULL);
  assert(VP8LAddVector != NULL);
  assert(VP8LAddVectorEq != NULL);
  assert(VP8LVectorMismatch != NULL);
  assert(VP8LBundleColorMap != NULL);
  assert(VP8LPredictorsSub[0] != NULL);
  assert(VP8LPredictorsSub[1] != NULL);
  assert(VP8LPredictorsSub[2] != NULL);
  assert(VP8LPredictorsSub[3] != NULL);
  assert(VP8LPredictorsSub[4] != NULL);
  assert(VP8LPredictorsSub[5] != NULL);
  assert(VP8LPredictorsSub[6] != NULL);
  assert(VP8LPredictorsSub[7] != NULL);
  assert(VP8LPredictorsSub[8] != NULL);
  assert(VP8LPredictorsSub[9] != NULL);
  assert(VP8LPredictorsSub[10] != NULL);
  assert(VP8LPredictorsSub[11] != NULL);
  assert(VP8LPredictorsSub[12] != NULL);
  assert(VP8LPredictorsSub[13] != NULL);
  assert(VP8LPredictorsSub[14] != NULL);
  assert(VP8LPredictorsSub[15] != NULL);
  assert(VP8LPredictorsSub_C[0] != NULL);
  assert(VP8LPredictorsSub_C[1] != NULL);
  assert(VP8LPredictorsSub_C[2] != NULL);
  assert(VP8LPredictorsSub_C[3] != NULL);
  assert(VP8LPredictorsSub_C[4] != NULL);
  assert(VP8LPredictorsSub_C[5] != NULL);
  assert(VP8LPredictorsSub_C[6] != NULL);
  assert(VP8LPredictorsSub_C[7] != NULL);
  assert(VP8LPredictorsSub_C[8] != NULL);
  assert(VP8LPredictorsSub_C[9] != NULL);
  assert(VP8LPredictorsSub_C[10] != NULL);
  assert(VP8LPredictorsSub_C[11] != NULL);
  assert(VP8LPredictorsSub_C[12] != NULL);
  assert(VP8LPredictorsSub_C[13] != NULL);
  assert(VP8LPredictorsSub_C[14] != NULL);
  assert(VP8LPredictorsSub_C[15] != NULL);
}

//------------------------------------------------------------------------------
