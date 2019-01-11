
#include <string>

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

#include <eosiolib/multi_index.hpp>

#include <eosio.token/eosio.token.hpp>


#include <eosiolib/time.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>

#include <eosiolib/privileged.h>
#include <eosiolib/transaction.hpp>




#include <cmath>
#include <map>


using eosio::permission_level;
using eosio::action;

using eosio::name;

using eosio::symbol_type;

using eosio::time_point_sec;


using eosio::asset;

using namespace eosio;

#define TOKEN_SYMBOL S(4, MC)    //代币符号

#define CONTRACTACC N(pppppppp1231) // owner of token contract

class betstake : public eosio::contract {

    static constexpr uint32_t refund_delay_sec = 24 * 3600;



    private:
	
	
	
		/// @abi table status i64
		struct status{
			uint64_t id;
			uint64_t staking;
			uint64_t unstaking;
			
			uint64_t primary_key()const { return id; }
			
			EOSLIB_SERIALIZE( status, (id)(staking)(unstaking) )
		};

        /// @abi table unstaking
        struct unstaking {
            account_name    owner;
            uint64_t           amount;
            time_point_sec  	request_time;

            uint64_t primary_key()const { return owner; }

            EOSLIB_SERIALIZE( unstaking, (owner)(amount)(request_time) )
        };




        /// @abi table staking i64
        struct staking {
            account_name owner;
            uint64_t        balance;

            bool is_empty() const { return !(balance); }
            uint64_t primary_key() const { return owner; }

            EOSLIB_SERIALIZE( staking, (owner)(balance) )
        };

		typedef eosio::multi_index< N(status), status> status_index;
        typedef eosio::multi_index< N(unstaking), unstaking> unstaking_index;
        typedef eosio::multi_index< N(staking), staking> staking_index;

		status_index    statuss;
        staking_index   stakes;
        unstaking_index unstakes;



    public:
	
		betstake(account_name self):eosio::contract(self),
			// 初始化列表
			stakes(_self, _self),
			unstakes(_self, _self),
			statuss(_self, _self)
		{}
		
		//@abi action
		void init(){
			require_auth(_self);
			auto globalvars_itr = statuss.begin();
			eosio_assert(globalvars_itr == statuss.end(), "Contract is init");
			statuss.emplace(_self, [&](auto& g){
				g.id = 1;
				g.staking = 0;
				g.unstaking = 0;
			});
		}
		
		// @abi action
		void transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
		{

            if (from == _self) {
				return;
			};
			require_auth( from );
			
			
			eosio_assert(symbol_type(TOKEN_SYMBOL)==quantity.symbol, "must transfer rush");
            eosio_assert( quantity.is_valid(), "Invalid asset");
            eosio_assert( quantity.amount > 0, "must stake positive quantity");

            auto itr = stakes.find(from);
            if( itr == stakes.end() ) {
                itr = stakes.emplace(_self, [&](auto& acnt){
                    acnt.owner = from;
					acnt.balance = quantity.amount;
                });
            }else{
				stakes.modify( itr, 0, [&]( auto& acnt ) {
					acnt.balance += quantity.amount;
				});
			};


			auto sta = statuss.find(1);
			eosio_assert(sta != statuss.end(),"must init first");
			statuss.modify( sta, 0, [&]( auto& acnt ) {
				acnt.staking += quantity.amount;
			});
			
        }

        // @abi action
        void unstfake(const account_name to, const asset& quantity) {
            require_auth(to);



            eosio_assert( quantity.is_valid(), "Invalid asset");
            eosio_assert( quantity.amount > 0, "must stake positive quantity");

            auto itr = stakes.find(to);
            eosio_assert(itr != stakes.end(), "unknown account");

            stakes.modify(itr, 0, [&](auto& acnt) {
                eosio_assert(acnt.balance >= quantity.amount, "insufficient balance");
                acnt.balance -= quantity.amount;
            });

            auto req_itr = unstakes.find(to);
            if(req_itr == unstakes.end()) {
                req_itr = unstakes.emplace(_self, [&](auto& acnt){
                    acnt.owner = to;
					acnt.amount = quantity.amount;
					acnt.request_time =  time_point_sec(now());
                });
            }else{
				unstakes.modify(req_itr, 0, [&](auto& acnt){
					acnt.amount += quantity.amount;
					acnt.request_time =  time_point_sec(now());
				});
			};
			
			auto sta = statuss.find(1);
			eosio_assert(sta != statuss.end(),"must init first");
			statuss.modify( sta, 0, [&]( auto& acnt ) {
				acnt.staking -= quantity.amount;
				acnt.unstaking += quantity.amount;
			});



		  eosio::transaction txn{};
		  txn.delay_sec = 60;
		  txn.actions.emplace_back( 
				permission_level{ _self, N(active) },
				_self,
				N(refund), 
				std::make_tuple(to));
		  txn.send(N(to),_self,true);


		 if(itr->is_empty()) {
			stakes.erase(itr);
		  };  

        }
		
		// @abi action
        void refund(const account_name to) { 
			require_auth(_self);
            auto req = unstakes.find(to);
			eosio_assert(req != unstakes.end(), "refund request not found");									
			asset money = asset(req->amount, TOKEN_SYMBOL);
			INLINE_ACTION_SENDER(eosio::token,transfer)(
				CONTRACTACC,{_self,N(active)},
				{ _self,to, money, std::string("unstake") } );
            unstakes.erase(req);
			auto sta = statuss.find(1);
			eosio_assert(sta != statuss.end(),"must init first");
			statuss.modify( sta, 0, [&]( auto& acnt ) {
				acnt.unstaking -= req->amount;
			});
        }
		
		
};

#define EOSIO_ABI_EX( TYPE, MEMBERS ) \
extern "C" { \
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
      auto self = receiver; \
      if( code == self || code == N(pppppppp1231)) { \
      	 if( action == N(transfer)){ \
      	 	eosio_assert( code == N(pppppppp1231), "Must transfer EOS"); \
      	 } \
         TYPE thiscontract( self ); \
         switch( action ) { \
            EOSIO_API( TYPE, MEMBERS ) \
         } \
         /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
      } \
   } \
}

EOSIO_ABI_EX( betstake, (transfer)(unstfake)(refund))
