from web3 import Web3

# 함수 시그니처
function_signature = "getRandomRelays(uint256)"

# keccak256 해시 계산
function_selector = Web3.keccak(text=function_signature).hex()

# 첫 4바이트만 사용
function_selector = function_selector[:10]

print(function_selector)

