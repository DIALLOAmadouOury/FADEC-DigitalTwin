/**
  ******************************************************************************
  * @file    network_data_params.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-05-05T16:38:32+0200
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#include "network_data_params.h"


/**  Activations Section  ****************************************************/
ai_handle g_network_activations_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(NULL),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};




/**  Weights Section  ********************************************************/
AI_ALIGNED(32)
const ai_u64 s_network_weights_array_u64[37] = {
  0x3eaf785abca4b940U, 0xbce66fa0bf369133U, 0x3ea3f352bf3b03edU, 0xbe64d8423af09958U,
  0xbec430773f0a308eU, 0x3ee93996bf39ed29U, 0xbe254e133abbaca4U, 0xbeaa83f53d63a697U,
  0x3f1d686f3e275354U, 0xbe84fa953e949f99U, 0xbf02b4633eec5c0cU, 0x3ed2030abe81cdfeU,
  0x25a58401bc95f2b4U, 0x25deace4be42ac13U, 0xa44dca6ebe27142bU, 0x24ef70d4be58fc16U,
  0x3ed167ce3e91afe6U, 0x3ee519623f02a361U, 0xbe5045e43e87c040U, 0x3e5a82483dced88dU,
  0x3f14f565bebbb382U, 0xbece2b44bee57246U, 0xbea18dbf3e56dd1fU, 0x3e3f0cc4bbe7eb32U,
  0xbe2ac244bc9a7575U, 0xbe8e43b63eb575e2U, 0x3cba7fe0bed176dbU, 0x3f144bb73ed2b5f1U,
  0xbea8fb1dbf0c77d0U, 0xbf1f53b53ea4e33eU, 0xbe0ab934be6ebaf5U, 0x3ef5be9a3eef25b1U,
  0xbd758c77bea3e898U, 0x3ed83bccbe9d082bU, 0xbf5d9263bf527bcfU, 0x3f1eebdabf00620bU,
  0x3eabc288U,
};


ai_handle g_network_weights_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(s_network_weights_array_u64),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};

