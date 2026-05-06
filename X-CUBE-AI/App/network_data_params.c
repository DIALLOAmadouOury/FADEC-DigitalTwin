/**
  ******************************************************************************
  * @file    network_data_params.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-05-06T18:09:11+0200
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
  0x3e0234c83f2569b1U, 0x3edfd42dbe018e02U, 0x3f86d835bf0d4743U, 0x3c1f14813f2e247bU,
  0xbeed7c1dbefd81ebU, 0x3f81be713e17debcU, 0xbd96118b3edeecb7U, 0x3ec95cdb3f1818d3U,
  0xbf61fe933e648914U, 0xbf36c4d4bf13c2eeU, 0xbdda10e03e69867cU, 0xbe6662143b837900U,
  0x3d4243d73e7401bfU, 0xbdadf387bd8eac06U, 0x3dea5be53dde6eccU, 0x0U,
  0xbf0fd9bf3d8574a8U, 0x3df2ea103e866e3eU, 0xbe0aba88bf006ba0U, 0x3edde6ae3e8c8226U,
  0xbeaf10cdbe194f58U, 0x3f01bd45bf0b11a5U, 0xbf0eae24bf026486U, 0x3ee732da3e5d280cU,
  0xbf06d88a3f04cdf9U, 0xc02a418f3dff199bU, 0x4000aa173e9bb99fU, 0x3cff5e803ef319faU,
  0x3f7641833eaa62a2U, 0x3fb7086bbef7ba7bU, 0xbfbbd6f03f4ba36fU, 0x3df74f583d02d7a0U,
  0x0U, 0x3d8fa2d23d985bf3U, 0x3efd72d8be0d5c4cU, 0x3fb4ffa8c003f756U,
  0x3d8f0516U,
};


ai_handle g_network_weights_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(s_network_weights_array_u64),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};

