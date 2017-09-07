#include <glua/lparsercombinator.h>
#include <cassert>
#include <iostream>
#include <stdarg.h>
#include <glua/glua_astparser.h>
#include <glua/exceptions.h>

namespace glua {
    namespace parser {

        static std::string VAR_T("var");
        static std::string FINAL_VAR_T("final_var");
        static std::string EMPTY_VAR_T("empty_var");

        VarP create_var(const std::string &name)
        {
            Var v;
            v.name = name;
            v.is_middle_var = false;
            v.is_not_left_var = false;
            v.origin_var = nullptr;
            v.min_count = 1;
            v.var_type = VAR_T;
            return std::make_shared<Var>(v);
        }

        VarP create_final_var(const std::string &name)
        {
            Var v;
            v.name = name;
            v.is_middle_var = false;
            v.is_not_left_var = false;
            v.origin_var = nullptr;
            v.min_count = 1;
            v.var_type = FINAL_VAR_T;
            return std::make_shared<Var>(v);
        }

        VarP create_empty_var()
        {
            VarP v = create_final_var(std::string(""));
            v->min_count = 0;
            v->var_type = EMPTY_VAR_T;
            return v;
        }

        ParseResult::ParseResult(ParserState s, MatchResult *r)
        {
            _state = s;
            _result = r;
        }

        ParserContext::~ParserContext()
        {
            for (auto it = _parser_functors.begin(); it != _parser_functors.end(); ++it)
            {
                ParserFunctor *fn = *it;
                if (nullptr != fn)
                    delete fn;
            }
            for (auto it = _match_results.begin(); it != _match_results.end(); ++it)
            {
                MatchResult *r = *it;
                if (nullptr != r)
                    delete r;
            }
        }

        ConcatParserFunctor::ConcatParserFunctor(ParserContext *ctx, std::vector<ParserFunctor*> &parsers)
        {
            _ctx = ctx;
            _parsers = parsers;
			for(const auto &item : _parsers)
			{
				item->set_parent_parser(this);
			}
        }

        FinalMatchResult::FinalMatchResult(Input &input){
            this->_next_input = input;
        }

        ComplexMatchResult::ComplexMatchResult(Input &input){
            this->_next_input = input;
        }

        MatchResult *MatchResult::invoke_parser_post_callback()
        {
			if (_parser)
				return _parser->invoke_post_callback(this);
			else
				return this;
        }

		size_t MatchResult::linenumber_after_end(MatchResult *parent_mr) const
        {
			if (!parent_mr || !parent_mr->is_complex())
				return last_token().linenumber - head_token().linenumber + 1;
			auto *parent_cmr = parent_mr->as_complex();
			for (size_t i = 0; i<parent_cmr->size(); ++i)
			{
				auto *item = parent_cmr->get_item(i);
				if (item->head_token().linenumber>this->head_token().linenumber)
				{
					return item->head_token().linenumber - this->head_token().linenumber;
				}
			}
			return last_token().linenumber - head_token().linenumber + 1;
        }

        ParseResult ParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
        {
            ParseResult pr(SUCCESS, nullptr);
            return pr;
        }

		ParseResult GluaTypeParserFunctor::tryParse(Input &input, size_t skip_end_size2, bool optional)
        {
			_ctx->set_last_match_token(input.read());
			ParserContextParsingLevelScope level_scope(_ctx);
			if (_ctx->parsing_level_overflow(&input))
			{
				return _ctx->failure_parser()->tryParse(input, 0, optional);
			}
			_ctx->expect_token(input, "type", optional, skip_end_size2);
			Input ninput(input);
			ninput.inc_pos(1);
			_ctx->expect_token_type(ninput, TOKEN_RESERVED::LTK_NAME, optional, skip_end_size2);
			auto type_name_token = ninput.read();
			ninput.inc_pos(1);

			// TODO: 这里要能从ctx里获取到 record_name的parserfunctor，然后进行parse
			return _ctx->failure_parser()->tryParse(input, 0, optional);
        }

        ParseResult ConcatParserFunctor::tryParse(Input &input, size_t skip_end_size2, bool optional)
        {
			_ctx->incr_parser_functor_executed_count("concat");
            _ctx->set_last_match_token(input.read());
            ParserContextParsingLevelScope level_scope(_ctx);
            if (_ctx->parsing_level_overflow(&input))
            {
                return _ctx->failure_parser()->tryParse(input, 0, optional);
            }
            assert(_parsers.size() > 0);
			if (node_name() == "namelist_in_record")
				printf("");
			if (node_name() == "record_with_generic")
				printf("");
			if (node_name() == "local_def_stat")
				printf("");
			// TODO: 做成expect
            if (_parsers.size() == 1)
            {
                auto pr = (*_parsers.begin())->tryParse(input, skip_end_size2, optional);
                after_match(input, pr);
                return pr;
            }
            auto parser_min_length = _ctx->parser_min_length(this);
            if (parser_min_length > 0 && !input.can_read(parser_min_length, skip_end_size2))
                return _ctx->failure_parser()->tryParse(input, 0, optional);
			auto it = 0;
			auto start_match_pos = input.pos(); // 开始匹配时候的token索引
			ParserFunctor *cur_parser = _parsers[it];
			// TODO: 当optional为true时，expect结果，直接构造error而不是failure
            // TODO: 限制后面至少跟着剩下_parsers需要的tokens，否则在 exp ::= exp binop exp 这种情况下，在匹配规则的第一个exp时，就会匹配到全部，然后接下来匹配失败
            // TODO: 还有一种方法是，这种情况下，多种结果的分支都作为options考虑
            size_t skip_end_size = parser_min_length - cur_parser->min_length();
            auto ninput_it = input.skip_end();
            ParseResult pr = cur_parser->tryParse(ninput_it, skip_end_size + skip_end_size2, optional);
            struct exit_scope {
                Input *_input;
                exit_scope(Input *in) : _input(in){}
                ~exit_scope() {
                    _input->reset_end_skipped();
                }
            } _exit_scope(&input);
            if (pr.state() == ParserState::SUCCESS)
            {
                ++it;
                while (it < _parsers.size())
                {
					cur_parser = _parsers[it];
                    skip_end_size -= cur_parser->min_length();
                    if (!pr.result()->next_input().can_read(cur_parser->min_length(), skip_end_size + skip_end_size2))
                    {
                        input.reset_end_skipped();
						auto maybe_error = std::string("expect at least ") + std::to_string(cur_parser->min_length()) + " tokens after "
							+ pr.result()->next_input().read().token;
						auto ninput = pr.result()->next_input();
						if(!optional)
						{
							_ctx->set_error(maybe_error);
							return _ctx->failure_parser()->tryParse(ninput, 0, optional);
						}
                        return _ctx->failure_parser()->tryParse(ninput, 0, optional);
                    }

					// 需要判断出input结束了，而当前parser的剩下parsers没有结束，需要非空数据，要提供合理的报错信息
					if(!pr.result()->next_input().can_read(1, skip_end_size + skip_end_size2))
					{
						// input读取完了时候
						size_t min_len_sum_of_remaining_parsers = 0;
						for(size_t k=it+1;k<_parsers.size();++k)
						{
							min_len_sum_of_remaining_parsers += _parsers[k]->min_length();
						}
						if(min_len_sum_of_remaining_parsers>0)
						{
							// 把_parsers中剩下的最可能的缺少的符号（单个literal符号优先级最高，没有就->str()）作为缺少的maybe error
							std::vector<ParserFunctor*> remaining_literal_parsers;
							std::string remaining_literal_names_str;
							for(size_t k=it+1;k<_parsers.size();++k)
							{
								auto parser = _parsers[k];
								if (parser->is_literal())
								{
									remaining_literal_parsers.push_back(parser);
									if (remaining_literal_names_str.length() > 0)
										remaining_literal_names_str += " or ";
									remaining_literal_names_str += "'" + parser->str() + "'";
								}
							}
							auto maybe_error = std::string("maybe missing ") + (remaining_literal_parsers.size() > 0 ? remaining_literal_names_str : (_parsers.size() > it + 1 ? _parsers[it + 1]->str() : "unknown symbol")) + " at end of file";
							_ctx->add_maybe_error(maybe_error);
							input.reset_end_skipped();
							if(!optional)
							{
								_ctx->set_error(maybe_error);
								return _ctx->error_parser()->tryParse(input, 0, optional);
							}
							return _ctx->failure_parser()->tryParse(input, 0, optional);
						}
					}

                    Input ninput = pr.result()->next_input();
                    ninput.reset_end_skipped();
                    auto lva=ninput.skip_end();
                    ParseResult pr2 = cur_parser->tryParse(lva, skip_end_size + skip_end_size2, optional);
                    if (pr2.state() == SUCCESS)
                    {
                        // TODO: keep old structure, or save origin structure result to new result
                        ComplexMatchResult *r = _ctx->create_complex_match_result(pr2.result()->next_input(), this);
                        if (pr.result()->is_complex() && (pr.result()->node_name().length() < 1 || pr.result()->as_complex()->items.size() == 0))
                        {
                            ComplexMatchResult *pr1_c = (ComplexMatchResult*) pr.result();
                            for (auto k = pr1_c->items.begin(); k != pr1_c->items.end(); ++k)
                            {
                                r->add_item(*k);
                            }
                        }
                        else
                        {
                            r->add_item(pr.result());
                        }
                        if (pr2.result()->is_complex() && (pr2.result()->node_name().length() < 1 || pr2.result()->as_complex()->items.size() == 0))
                        {
                            ComplexMatchResult *pr2_c = (ComplexMatchResult*)pr2.result();
                            for (auto k = pr2_c->items.begin(); k != pr2_c->items.end(); ++k)
                            {
                                r->add_item(*k);
                            }
                        }
                        else
                        {
                            r->add_item(pr2.result());
                        }
                        pr.set_result(r);
                    }
                    else if (pr2.state() == ParserState::FAILURE)
                    {

						auto error_line = (ninput.prev_token().linenumber > _ctx->inner_lib_code_lines() - 2) ? (ninput.prev_token().linenumber - _ctx->inner_lib_code_lines() + 2) : ninput.prev_token().linenumber;
						std::string maybe_error = std::string("parse error around ") + ninput.read(skip_end_size).token + " in line " + std::to_string(error_line);

						// 在这里猜测可能错误原因，如果不是正常的推导过程，加入maybe_errors
						// 如果此concat_parser以literal开头，并且上一级是union parser，且此literal开头在上一级union parser中唯一，构造maybe error
						// 如果此concat_parser以literal开头，并且上一级是union parser，且此literal开头在上一级union parser中不唯一但是满足这个开头的所有子项都匹配失败或者当前是最后一个在匹配的这个开头的parser，构造maybe error(考虑合并开头后匹配)
                        if(node_name().length()>0 && _parsers.size()>0 && _parsers[0]->is_literal() && parent_parser() && parent_parser()->is_union())
                        {
							auto head_literal_name = _parsers[0]->str(); // 头部的literal parser的符号名
							auto parent_union_parser = (UnionParserFunctor*) parent_parser();
							const auto &parent_parsers = parent_union_parser->parsers();
							auto index_in_parent = parent_union_parser->find_index_of_parser(this);
							if (index_in_parent >= 0)
							{
								bool has_matching_same_literal = false;
								// TODO: 如果剩下的同级parser也都失败，并且起始是literal，挑最长的加入maybe error
								for(size_t k=(size_t) index_in_parent+1;k<parent_parsers.size();++k)
								{
									auto item = parent_parsers[k];
									if(item->is_concat())
									{
										auto item_concat = (ConcatParserFunctor*) item;
										if(item_concat->size()>0 && item_concat->parsers()[0]->is_literal() && item_concat->parsers()[0]->str() == head_literal_name)
										{
											// FIXME: 考虑这里的性能影响
											
											// 判断能匹配的长度和this能判断的长度哪个长，如果item能匹配的更短，则忽略
											auto matched_tokens_count = ninput.pos() - start_match_pos + 1;
											auto item_matched_tokens_count = _ctx->parse_with_cache(item_concat, ninput, skip_end_size, optional).result()->next_input().pos() - start_match_pos;
											if(item_matched_tokens_count>matched_tokens_count)
												has_matching_same_literal = true;
											
											//has_matching_same_literal = true;
										}
									}
								}
								if (!has_matching_same_literal)
								{
									if (ninput.prev_token().linenumber > _ctx->inner_lib_code_lines() + 1)
									{
										std::stringstream error_ss;
										if (cur_parser->is_literal())
										{
											error_ss << "missing " << cur_parser->str() << " in line " << error_line << " but got " << ninput.read().token;
										}
										else
										{
											error_ss << "not end code or wrong structure of " << node_name() << " in line " << error_line;
										}
										maybe_error = error_ss.str();
										_ctx->add_maybe_error(maybe_error);
									}
								}
							}
                        }
						_ctx->set_last_error_token(input.read());
						if (!optional)
						{
							_ctx->set_error(maybe_error);
							return _ctx->error_parser()->tryParse(ninput, skip_end_size, optional);
						}
                        return pr2;
                    }
                    else
                    {
                        _ctx->set_last_error_token(input.read());
                        return pr2;
                    }
                    ++it;
                }
                after_match(input, pr);
                if (pr.state() == ParserState::SUCCESS && this->ignore_parsing_level_when_matched())
                {
                    _ctx->set_parsing_level(1);
                }
                return pr;
            }
            else if (pr.state() == ParserState::FAILURE)
            {
                return pr;
            }
            else
            {
                return pr;
            }
        }

		ParseResult SubtractionParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
        {
			// _ctx->set_last_error_token(input.read());
			_ctx->set_last_match_token(input.read());
			assert(nullptr != _main_parser);
			ParseResult pr = _ctx->parse_with_cache(_main_parser, input, 0, optional);
			if (pr.state() != ParserState::SUCCESS)
				return pr;
			if (_subtrac_parsers.size() < 1)
				return pr;
			for(const auto &subtrac_parser : _subtrac_parsers)
			{
				ParseResult pr_of_subtrac = _ctx->parse_with_cache(subtrac_parser, input, 0, optional);
				if(pr_of_subtrac.state() == ParserState::SUCCESS)
				{
					return _ctx->failure_parser()->tryParse(input, 0, optional);
				}
			}
			return pr;
        }

        ParseResult RepeatStarParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
        {
            // _ctx->set_last_error_token(input.read());
            _ctx->set_last_match_token(input.read());
            assert(nullptr != _parser);
            Input cur_input = input;
            size_t max_cycle = 10086;
            size_t cycle_count = 0;
            ParseResult result_pr = _ctx->empty_parser()->tryParse(input, skip_end_size, optional);
            result_pr.result()->set_parser(this);
            while (cycle_count < max_cycle)
            {
                ++cycle_count;
                ParseResult pr = _parser->tryParse(cur_input, skip_end_size, optional);
                if (pr.state() == ParserState::ERROR)
                    return pr;
                if (pr.state() == ParserState::FAILURE)
                {
                    return result_pr;
                }
                // merge pr to result_pr
                ComplexMatchResult *result_mr = (ComplexMatchResult*)result_pr.result();
                result_mr->add_item(pr.result());
                Input ninput = pr.result()->next_input();
                result_mr->set_input(ninput);
                ParseResult pr2 = _parser->tryParse(ninput, skip_end_size, optional);
                if (pr2.state() != SUCCESS)
                {
                    after_match(input, result_pr);
                    return result_pr;
                }
                result_mr->add_item(pr2.result());
                cur_input = pr2.result()->next_input();
                result_mr->set_input(cur_input);
            }
            return _ctx->failure_parser()->tryParse(input, 0, optional);
        }

        ParseResult RepeatCountParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
        {
            // _ctx->set_last_error_token(input.read());
            _ctx->set_last_match_token(input.read());
            assert(nullptr != _parser);
            if (_num < 0)
            {
                auto pr = _ctx->empty_parser()->tryParse(input, skip_end_size, optional);
                after_match(input, pr);
                return pr;
            }
            Input cur_input = input;
            size_t cycle_count = 0;
            ParseResult result_pr = _ctx->empty_parser()->tryParse(input, skip_end_size, optional);
            result_pr.result()->set_parser(this);
            while (cycle_count < _num)
            {
                ++cycle_count;
                ParseResult pr = _parser->tryParse(cur_input, skip_end_size, optional);
                if (pr.state() == ERROR)
                    return pr;
                if (pr.state() == FAILURE)
                {
                    return result_pr;
                }
                // merge pr to result_pr
                ComplexMatchResult *result_mr = (ComplexMatchResult*)result_pr.result();
                result_mr->add_item(pr.result());
                Input ninput = pr.result()->next_input();
                result_mr->set_input(ninput);
                if (cycle_count == _num)
                {
                    after_match(input, result_pr);
                    return result_pr;
                }
                cur_input = ninput;
            }
            return _ctx->failure_parser()->tryParse(input, 0, optional);
        }

        ParserFunctor *UnionParserFunctor::lookahead()
        {
            this->_parsers_started_with_final.clear();
            this->_parsers_not_started_with_final.clear();
            for (auto i = _parsers.begin(); i != _parsers.end(); ++i)
            {
                auto *p = *i;
                if (p->is_concat())
                {
                    auto *head = p->head();
                    if (nullptr != head && head->is_literal())
                    {
                        if (head->str().length() > 0)
                        {
                            auto found1 = _parsers_started_with_final.find(head->str());
                            std::shared_ptr<std::vector<ParserFunctor*>> list;
                            if (found1 == _parsers_started_with_final.end())
                            {
                                list = std::make_shared<std::vector<ParserFunctor*>>();
                                _parsers_started_with_final[head->str()] = list;
                            }
                            else
                                list = found1->second;
                            list->push_back(p);
                            continue;
                        }
                    }
                }
                _parsers_not_started_with_final.push_back(p);
            }
            this->_lookahead_optimized = true;
            return this;
        }

        bool UnionParserFunctor::remove_direct_left_recur_rule(std::vector<ParserFunctor*> &result_parsers)
        {
            // TODO: test
            bool has_direct_left_recur = false;
            std::vector<ParserFunctor*> direct_left_recur_items;
            std::vector<ParserFunctor*> not_direct_left_recur_items;
            for (auto i = _parsers.begin(); i != _parsers.end(); ++i)
            {
                ParserFunctor *ptr = *i;
                if (ptr->is_cached())
                {
                    auto *cached_p = (CachedParserFunctor*) ptr;
                    assert(cached_p->exist());
                    auto *real_p = _ctx->get_cached(cached_p->name());
                    assert(nullptr != real_p);
                    ptr = real_p;
                }
                auto *head = ptr->head();
                if (head == this)
                {
                    has_direct_left_recur = true;
                    direct_left_recur_items.push_back(ptr);
                }
                else
                    not_direct_left_recur_items.push_back(ptr);
            }
            if (!has_direct_left_recur)
            {
                result_parsers.push_back(this);
                return false;
            }
            // remove direct-left-recur
            // group by direct-left-recur and not-direct-left-recur
            assert(!(direct_left_recur_items.size() > 0 && not_direct_left_recur_items.size() < 1));
            ParserFunctor *left_item_parser = nullptr;	// DestVar' in DestVar => DestVar + DestVar'
            ParserFunctor *not_left_item_parser = nullptr; // DestVar'' in DestVar => DestVar''
            // DestVar => DestVar + DestVar' | DestVar''======> DestVar = > DestVar'' + P' and P' = > DestVar' + P' | ε
            ParserFunctor *p_parser = nullptr; // P'
            left_item_parser = _ctx->concat_parsers(direct_left_recur_items);
            not_left_item_parser = _ctx->concat_parsers(not_direct_left_recur_items);
            std::string p_parser_name = std::to_string((intptr_t)left_item_parser) + std::to_string((intptr_t)not_left_item_parser) + "_left_process_p";
            p_parser = _ctx->cache(p_parser_name.c_str(), _ctx->optional_parser(_ctx->p_and(left_item_parser, _ctx->named_parser(p_parser_name.c_str()))));
            // TODO: use name to replace all use of old DestVar
            this->_parsers.clear();
            this->_parsers.push_back(_ctx->p_and(not_left_item_parser, p_parser));
            result_parsers.push_back(left_item_parser);
            result_parsers.push_back(not_left_item_parser);
            result_parsers.push_back(p_parser);
            result_parsers.push_back(this);
            return true;
        }

        void UnionParserFunctor::finish()
        {
            // remove cache, sort branches, etc.
            // sort branches, shortest not-direct-left-recur branch sort first
            // TODO: indirect left-recur remove
            std::vector<ParserFunctor*> fetched_parsers;
            for (auto i = _parsers.begin(); i != _parsers.end(); ++i)
            {
                auto *p = *i;
                assert(nullptr != p);
                if (p->is_cached())
                {
                    auto *cached_p = (CachedParserFunctor*) p;
                    assert(cached_p->exist());
                    fetched_parsers.push_back(_ctx->get_cached(cached_p->name()));
                }
                else
                    fetched_parsers.push_back(p);
            }
			auto sort_func = [&](ParserFunctor *p1, ParserFunctor *p2) {
				// fetch head and length, and then sort
				if (p1 == p2)
					return false;
				auto *head1 = p1->head();
				auto *head2 = p2->head();
				if (this == head2)
					return false;
				if (this == head1)
					return true;
				if (p1->size() != p2->size())
					return p1->size() < p2->size();
				size_t pos1 = 0;
				size_t pos2 = 0;
				auto it = _parsers.begin();
				while (it != _parsers.end())
				{
					if (*it == p1)
						break;
					++it;
					++pos1;
				}
				it = _parsers.begin();
				while (it != _parsers.end())
				{
					if (*it == p2)
						break;
					++it;
					++pos2;
				}
				return pos1 < pos2;
			};
			std::sort(fetched_parsers.begin(), fetched_parsers.end(), sort_func);
            std::vector<ParserFunctor*> removed_direct_left_recur_parsers;
            std::vector<UnionParserFunctor*> union_sub_parsers;
            for (auto i = fetched_parsers.begin(); i != fetched_parsers.end(); ++i)
            {
                auto *p = *i;
                /*
                Ai→Ajγ
                =>
                Ai→δ1γ /δ2γ /…/δkγ
                =>
                remove direct left recur
                */
                if (p->is_concat())
                {
                    ConcatParserFunctor *concat_p = (ConcatParserFunctor*) p;
                    auto *head = concat_p->head();
                    auto tail_items = concat_p->tail();
                    if (head && head->is_union() && tail_items.size() > 0)
                    {
                        UnionParserFunctor *head_p = (UnionParserFunctor*) head;

                        std::vector<ParserFunctor*> expand_items;
                        for (auto k = head_p->_parsers.begin(); k != head_p->_parsers.end(); ++k)
                        {
                            auto *ptr = *k;
                            std::vector<ParserFunctor*> sub_items;
                            sub_items.push_back(ptr);
                            for (auto h = tail_items.begin(); h != tail_items.end(); ++h)
                            {
                                sub_items.push_back(*h);
                            }
                            expand_items.push_back(_ctx->concat_parsers(sub_items));
                        }
                        auto *expand_p = (UnionParserFunctor*)_ctx->union_parsers(expand_items);
                        expand_p->remove_direct_left_recur_rule(removed_direct_left_recur_parsers);
                    }
                    else
                        removed_direct_left_recur_parsers.push_back(p);
                }
                else if (p->is_union())
                {
                    UnionParserFunctor *union_p = (UnionParserFunctor*) p;
                    union_sub_parsers.push_back(union_p);
                    std::vector<ParserFunctor*> to_union_items;
                    for (auto j = union_p->_parsers.begin(); j != union_p->_parsers.end(); ++j)
                    {
                        auto *sub_p = *j;
                        if (sub_p->is_concat())
                        {
                            auto *concat_p = (ConcatParserFunctor*) sub_p;
                            auto *head = concat_p->head();
                            auto tail_items = concat_p->tail();
                            if (head && head->is_union() && tail_items.size() > 0)
                            {
                                UnionParserFunctor *head_p = (UnionParserFunctor*) head;
                                std::vector<ParserFunctor*> expand_items;
                                for (auto k = head_p->_parsers.begin(); k != head_p->_parsers.end(); ++k)
                                {
                                    auto *ptr = *k;
                                    std::vector<ParserFunctor*> sub_items;
                                    sub_items.push_back(ptr);
                                    for (auto h = tail_items.begin(); h != tail_items.end(); ++h)
                                    {
                                        sub_items.push_back(*h);
                                    }
                                    expand_items.push_back(_ctx->concat_parsers(sub_items));
                                }
                                auto *expand_p = (UnionParserFunctor*)_ctx->union_parsers(expand_items);
                                expand_p->remove_direct_left_recur_rule(to_union_items);
                            }
                            else
                                to_union_items.push_back(p);
                        }
                        else
                            to_union_items.push_back(p);
                    }
                    auto *expand_unioned_items = _ctx->union_parsers(to_union_items);
                    removed_direct_left_recur_parsers.push_back(expand_unioned_items);
                }
                else
                    removed_direct_left_recur_parsers.push_back(p);
            }
            // TODO: find cycle left-recur
            // _parsers = fetched_parsers;
            _parsers = removed_direct_left_recur_parsers;
        }

        void ParserContext::finish()
        {
            auto saved_parser_functors = _parser_functors; // when finish sub parser functor, the _parser_functors will change, so save it first
            for (auto i = saved_parser_functors.begin(); i != saved_parser_functors.end(); ++i)
            {
                auto *p = *i;
                p->finish();
            }
        }

        std::string ParserContext::simple_token_error() const
        {
            if (_last_error_token.position >= _last_match_token.position || _last_second_error_token.position >= _last_match_token.position)
            {
                auto error_str = simple_token_error(_last_error_token);
                if (_last_second_error_token.position > _last_error_token.position)
                    error_str += "\n" + simple_token_error(_last_second_error_token, false);
                return error_str;
            }
            else {
                return simple_token_error(_last_match_token);
            }
        }

        std::string ParserContext::simple_token_error(GluaParserToken token, bool with_maybe_errors) const
        {
            // TODO: 输出错误代码周边的代码
            std::stringstream ss;
            ss << "error not match after token " << token.token
                << ", line " << (token.linenumber-7+2) // FIXME： 这里暂时直接扣除内置库的代码行数
                << ", position " << token.position;
			if (with_maybe_errors)
			{
				if (_maybe_errors.size() > 0)
					ss << "\nmaybe errors: ";
				for (const auto &item : _maybe_errors)
				{
					ss << "\n" << item;
				}
			}
            return ss.str();
        }

        bool ParserContext::parsing_level_overflow(Input *input)
        {
            // whether nested parser calling too deeper
            auto result = _parsing_level > PARSING_MAX_LEVEL_DEPTH;
            if (result)
            {
                _last_error_token = input->read();
                _last_match_token = input->read();
            }
            return result;
        }

        ParserFunctor *ParserContext::cache(std::string name, ParserFunctor *parser)
        {
            _named_parser_functors.erase(name);
            _named_parser_functors[name] = parser;
            if (parser->node_name().length() < 1)
                parser->set_node_name(name);
            return parser;
        }

        ParserFunctor *ParserContext::get_cached(std::string name)
        {
            auto found = _named_parser_functors.find(name);
            if (found != _named_parser_functors.end())
            {
                return found->second;
            }
            else
                return nullptr;
        }

        size_t ParserContext::parser_min_length(ParserFunctor *parser)
        {
            int len = get_parser_min_length_cached(parser);
            if (len >= 0)
                return len;
			int magic_len = -10086;
            if (len == magic_len)
                return 0; // the parser is now fetching min length, use this to avoid recur call self
            set_parser_min_length(parser, static_cast<size_t>(magic_len)); // FIXME: 这里可能有BUG，魔法数是size_t(负数)，也就是非常大的数
            size_t mlen = parser->min_length();
            set_parser_min_length(parser, mlen);
            return mlen;
        }

        ParseResult ParserContext::parse_with_cache(ParserFunctor *parser, Input &input, size_t skip_end_size, bool optional)
        {
            size_t MAX_SIZE_AFTER_MAX_TOKENS_COUNT = 100000; // TODO： 这里要用input的pos和skip_end_size来cache，暂时用pos + skip_end_size * MAX_SIZE_AFTER_MAX_TOKENS_COUNT
            size_t size_key = input.pos() + skip_end_size * MAX_SIZE_AFTER_MAX_TOKENS_COUNT;
            auto found = _parsers_parse_result_cache.find(parser);
            if (found == _parsers_parse_result_cache.end())
            {
                _parsers_parse_result_cache[parser] = std::make_shared<std::unordered_map<size_t, ParseResult>>();
                found = _parsers_parse_result_cache.find(parser);
            }
            auto parser_result_map = found->second;
            auto found2 = parser_result_map->find(size_key);
            if (PARSING_MAX_LEVEL_DEPTH > 10 && _parsing_level > PARSING_MAX_LEVEL_DEPTH - 5)
            {
                // 如果是因为parsing_level导致的failure，不应该记录到cache
                return parser->tryParse(input, skip_end_size, optional);
            }
            else if (found2 == parser_result_map->end())
            {
                auto pr = parser->tryParse(input, skip_end_size, optional);
                (*parser_result_map)[size_key] = pr;
                return pr;
            }
            else
            {
                return found2->second;
            }
        }

		ParserFunctor *ParserFunctor::set_result_requirement(std::function<bool(Input &, MatchResult*)> fn)
		{
			if (!_result_requirement)
				_result_requirement = fn;
			else if (fn)
			{
				auto old_fn = _result_requirement;
				_result_requirement = [old_fn, fn](Input &input, MatchResult *mr)
				{
					return old_fn(input, mr) && fn(input, mr);
				};
			}
			return this;
		}

		ParserFunctor *ParserFunctor::set_next_token_types_in_types_or_newline(std::vector<int> types)
		{
			set_result_requirement([types](Input &input, MatchResult *mr)
			{
				auto token = input.token_after_position(mr->last_token().position);
				for (const auto &item : types)
				{
					if (token.type == item)
						return true;
				}
				if (token.type == TOKEN_RESERVED::LTK_EOS)
					return true;
				if (token.linenumber > mr->last_token().linenumber)
					return true;
				return false;
			});
			return this;
		}

		ParserFunctor *ParserFunctor::set_items_in_same_line()
		{
			set_result_requirement([](Input &input, MatchResult* mr) {
				if (mr->is_complex())
				{
					auto *cmr = mr->as_complex();
					if (cmr->size() >= 2)
					{
						size_t linenumber;
						auto it = cmr->items.begin();
						linenumber = (*it)->head_token().linenumber;
						++it;
						for (; it != cmr->items.end(); ++it)
						{
							auto *item = *it;
							if (item->head_token().linenumber != linenumber)
								return false;
						}
					}
				}
				return true;
			});
			return this;
		}

		void ParserFunctor::after_match(Input &input, ParseResult &pr)
		{
			if (pr.state() == ParserState::SUCCESS)
			{
				if (pr.result()->node_name().length() < 1)
				{
					pr.result()->set_node_name(node_name());
				}
				if (_result_requirement)
				{
					if (!_result_requirement(input, pr.result()))
						pr.set_state(ParserState::FAILURE);
				}
				//if (_post_callback)
				//	_post_callback(this, pr);
			}
		}

		ParserBuilder* ParserBuilder::lua_and(ParserFunctor *other1, ParserFunctor *other2,
			ParserFunctor *other3, ParserFunctor *other4)
		{
			if (_result)
				_result = _ctx-> p_and (_result, other1);
			else
				_result = other1;
			if (other2)
				_result = _ctx-> p_and (_result, other2);
			if (other3)
				_result = _ctx-> p_and (_result, other3);
			if (other4)
				_result = _ctx-> p_and (_result, other4);
			return this;
		}

		ParserBuilder* ParserBuilder::lua_or (ParserFunctor *other1, ParserFunctor *other2,
			ParserFunctor *other3, ParserFunctor *other4)
		{
			if (_result)
				_result = _ctx-> p_or (_result, other1);
			else
				_result = other1;
			if (other2)
				_result = _ctx-> p_or (_result, other2);
			if (other3)
				_result = _ctx-> p_or (_result, other3);
			if (other4)
				_result = _ctx-> p_or (_result, other4);
			return this;
		}

		ParseResult FailureParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional) {
			_ctx->set_last_match_token(input.read());
			ComplexMatchResult *r = _ctx->create_complex_match_result(input, this);
			ParseResult pr(ParserState::FAILURE, r);
			return pr;
		}

		ParseResult ErrorParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
        {
			_ctx->set_last_match_token(input.read());
			ComplexMatchResult *r = _ctx->create_complex_match_result(input, this);
			ParseResult pr(ParserState::ERROR, r);
			return pr;
        }

		size_t ConcatParserFunctor::min_length() const
		{
			size_t s = 0;
			for (auto i = _parsers.begin(); i != _parsers.end(); ++i)
			{
				s += _ctx->parser_min_length(*i);
			}
			return s;
		}

		/*std::string ConcatParserFunctor::str() const
		{
			std::stringstream ss;
			size_t c = 0;
			ss << "{";
			for (auto i = _parsers.begin(); i != _parsers.end(); ++i)
			{
				if (c > 0)
					ss << " ";
				ss << (*i)->str();
				++c;
			}
			ss << "}";
			return ss.str();
		}*/

		std::vector<std::string> ConcatParserFunctor::str_lines() const
        {
			std::vector<std::string> lines;
			lines.push_back(std::string("and ")+node_name());
			for(const auto &p : _parsers)
			{
				const auto &p_lines = p->str_lines();
				for(const auto &line : p_lines)
				{
					lines.push_back(std::string("\t") + line);
				}
			}
			return lines;
        }

		/*std::string SubtractionParserFunctor::str() const
		{
			std::stringstream ss;
			ss << _main_parser->str();
			for (const auto &parser : _subtrac_parsers)
			{
				ss << " - " << parser->str();
			}
			return ss.str();
		}*/

		std::vector<std::string> SubtractionParserFunctor::str_lines() const
        {
			std::vector<std::string> lines;
			lines.push_back(std::string("- ") + node_name());
			const auto &main_parser_lines = _main_parser->str_lines();
			for(const auto &line : main_parser_lines)
			{
				lines.push_back(std::string("\t") + line);
			}
			for(const auto &p : _subtrac_parsers)
			{
				const auto &p_lines = p->str_lines();
				for(const auto &line : p_lines)
				{
					lines.push_back(std::string("\t") + line);
				}
			}
			return lines;
        }

		std::vector<std::string> RepeatStarParserFunctor::str_lines() const
        {
			std::vector<std::string> lines;
			lines.push_back(std::string("* ") + node_name());
			const auto &p_lines = _parser->str_lines();
			for(const auto &line : p_lines)
			{
				lines.push_back(std::string("\t") + line);
			}
			return lines;
        }

		std::vector<std::string> RepeatCountParserFunctor::str_lines() const
		{
			std::vector<std::string> lines;
			lines.push_back(std::string("repeat ") + std::to_string(_num) + " " + node_name());
			const auto &p_lines = _parser->str_lines();
			for (const auto &line : p_lines)
			{
				lines.push_back(std::string("\t") + line);
			}
			return lines;
		}

		UnionParserFunctor::UnionParserFunctor(ParserContext *ctx, std::vector<ParserFunctor*> &parsers)
			: _ctx(ctx), _lookahead_optimized(false) {
			for (auto i = parsers.begin(); i != parsers.end(); ++i)
			{
				ParserFunctor *p = *i;
				if (p)
				{
					_parsers.push_back(p);
					p->set_parent_parser(this);
				}
				else
					_parsers.push_back(this);
			}
		}

		size_t UnionParserFunctor::min_length() const
		{
			size_t m = -1;
			for (auto i = _parsers.begin(); i != _parsers.end(); ++i)
			{
				size_t m2 = _ctx->parser_min_length(*i);
				if (m < 0 || m2 < m)
					m = m2;
			}
			if (m < 0)
				m = 0;
			return m;
		}

		/*std::string UnionParserFunctor::str() const
		{
			if (_parsers.size() < 1)
				return "";
			std::stringstream ss;
			size_t i = 0;
			ss << "{";
			for (auto it = _parsers.begin(); it != _parsers.end(); ++it)
			{
				if (i > 0)
					ss << "|";
				auto *p = *it;
				ss << p->str();
				++i;
			}
			ss << "}";
			return ss.str();
		}*/

		std::vector<std::string> UnionParserFunctor::str_lines() const
        {
			std::vector<std::string> lines;
			lines.push_back(std::string("or ") + node_name());
			for(const auto &p : _parsers)
			{
				const auto &p_lines = p->str_lines();
				for(const auto &line : p_lines)
				{
					lines.push_back(std::string("\t") + line);
				}
			}
			return lines;
        }

		ParseResult LiteralParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
		{
			if (input.can_read(1, skip_end_size))
			{
				GluaParserToken token = input.read(skip_end_size);
				if (token.token == _token_str)
				{
					Input ninput = input;
					ninput.inc_pos(1);
					FinalMatchResult *r = _ctx->create_final_match_result(ninput, this);
					r->token = token;
					ParseResult pr(SUCCESS, r);
					after_match(input, pr);
					return pr;
				}
			}
			return _ctx->failure_parser()->tryParse(input, 0, optional);
		}

		ParseResult NumberParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
		{
			if (input.can_read(1, skip_end_size))
			{
				GluaParserToken token = input.read(skip_end_size);
				if (token.type == LTK_INT || token.type == LTK_FLT)
				{
					Input ninput = input;
					ninput.inc_pos(1);
					FinalMatchResult *r = _ctx->create_final_match_result(ninput, this);
					r->token = token;
					ParseResult pr(SUCCESS, r);
					after_match(input, pr);
					return pr;
				}
			}
			return _ctx->failure_parser()->tryParse(input, 0, optional);
		}

		ParseResult StringParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
		{
			if (input.can_read(1, skip_end_size))
			{
				GluaParserToken token = input.read(skip_end_size);
				if (token.type == LTK_STRING)
				{
					Input ninput = input;
					ninput.inc_pos(1);
					FinalMatchResult *r = _ctx->create_final_match_result(ninput, this);
					r->token = token;
					ParseResult pr(SUCCESS, r);
					after_match(input, pr);
					return pr;
				}
			}
			return _ctx->failure_parser()->tryParse(input, 0, optional);
		}

		ParseResult LuaSymbolParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
		{
			if (input.can_read(1, skip_end_size))
			{
				GluaParserToken token = input.read(skip_end_size);
				if (token.type == LTK_NAME)
				{
					Input ninput = input;
					ninput.inc_pos(1);
					FinalMatchResult *r = _ctx->create_final_match_result(ninput, this);
					r->token = token;
					ParseResult pr(SUCCESS, r);
					after_match(input, pr);
					return pr;
				}
			}
			return _ctx->failure_parser()->tryParse(input, 0, optional);
		}

		ParseResult LuaTokenTypeParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
        {
			if (input.can_read(1, skip_end_size))
			{
				GluaParserToken token = input.read(skip_end_size);
				if (token.type == _type)
				{
					Input ninput = input;
					ninput.inc_pos(1);
					FinalMatchResult *r = _ctx->create_final_match_result(ninput, this);
					r->token = token;
					ParseResult pr(SUCCESS, r);
					after_match(input, pr);
					return pr;
				}
			}
			return _ctx->failure_parser()->tryParse(input, 0, optional);
        }

		/**
			TODO: 把手动解构做成自动解构,或者根据原始语法自动生成类
		    if exp then block
			{ elseif exp then block }
			[ else block ]
			end
		 */
		MatchResult *ParserContext::make_if_ast_node(ParserFunctor *pf, MatchResult *mr)
        {
			std::vector<GluaConditionNodeInfo> condition_node_infos;
			MatchResult *else_block_mr = nullptr;
			assert(mr->is_complex());
			auto cmr = mr->as_complex();
			assert(cmr->size() >= 5);
			condition_node_infos.push_back(std::make_pair(cmr->get_item(1), cmr->get_item(3)));
			for(size_t cur_pos=4;cur_pos<cmr->size()-1;++cur_pos)
			{
				MatchResult *cur_mr = cmr->get_item(cur_pos);
				auto head_token = cur_mr->head_token().token;
				if(head_token == "elseif")
				{
					assert(cur_mr->is_complex() && cur_mr->as_complex()->size()>=4);
					auto cur_cmr = cur_mr->as_complex();
					condition_node_infos.push_back(std::make_pair(cur_cmr->get_item(1), cur_cmr->get_item(3)));
				}
				else if(head_token == "else")
				{
					else_block_mr = cmr->get_item(cur_pos+1);
					break;
				}
				else
				{
					// end
					break;
				}
			}
			auto *node = new GluaIfStatNode(mr, condition_node_infos, else_block_mr);
			node->set_node_name("if_stat");
			add_match_result(node);
			return node;
        }

        ParserFunctor *ParserContext::generate_glua_parser()
        {
            auto *ctx = this;
            auto l = [&](std::string t) {
                return ctx->literal(t);
            };
            auto n = [&](std::string name) {
                return ctx->named_parser(name);
            };
            auto s = l;
            auto str_parser = ctx->string_parser();
            std::vector<glua::parser::ParserFunctor*> expr1_stru = { ctx->lua_symbol_parser(), l("("), str_parser, l(")") };
            glua::parser::ParserFunctor *expr_parser1 = ctx->concat_parsers(expr1_stru);
            std::vector<ParserFunctor*> unops = { l("-"), l("not"), l("#"), l("~") };
            ctx->cache("unop", ctx->union_parsers(unops));
            std::vector<ParserFunctor*> binops = { l("+"), l("-"), l("*"), l("/"), l("//"), l("^"),
                l("%"), l("&"), l("~"), l("|"), l(">>"), l("<<"), l(".."), l("<="),
                l("<"), l(">="), l(">"), l("=="), l("~="), l("and"), l("or") };
            ctx->cache("binop", ctx->union_parsers(binops));
            ctx->cache("fieldsep", ctx->p_or(s(","), s(";")));
            ctx->cache("field", ctx->p_or(
                ctx->p_and(s("["), n("exp"), s("]"), s("="), n("exp"))->as("array_index_field"),
                ctx->p_and(ctx->lua_symbol_parser(), ctx->p_or(s("="), s(":")), n("exp"))->as("map_index_field"),
				ctx->p_and(ctx->string_parser(), s(":"), n("exp"))->as("string_map_index_field"),
                n("exp")
                ));
            ctx->cache("var_declare_prefix", ctx->p_or(
                s("local"),
                s("var"),
                s("let")
                ));
            ctx->cache("fieldlist",
				ctx->repeatPlus(ctx->p_and(n("field"), ctx->optional_parser(n("fieldsep")))->as("fieldsep_value_pair")));
            ctx->cache("tableconstructor",
                ctx->p_or(
					ctx->p_and(s("{"), s("}")),
					ctx->p_and(s("{"), n("fieldlist"), s("}")),
					ctx->p_and(s("["), s("]")),
					ctx->p_and(s("["), n("fieldlist"), s("]"))
				));
            ctx->cache("parlist",
                ctx->p_or(s("..."), ctx->p_and(n("namelist"), ctx->optional_parser(ctx->p_and(s(","), s("..."))))));
            ctx->cache("funcbody_args_def",
                ctx->p_and(s("("), ctx->optional_parser(n("parlist")), s(")")));
            ctx->cache("funcbody",
                ctx->p_and(n("funcbody_args_def"), n("block"), s("end")));
            ctx->cache("functiondef",
				ctx->p_and(s("function"), n("funcbody")));
			ctx->cache("un_exp", ctx-> p_and(n("unop"), n("simpleexp"))->set_items_in_same_line());
            ctx->cache("simpleexp",
                ctx->p_or(
                s("nil"), s("false"), s("true"),
                s("..."), n("functiondef"),
                ctx->number_parser(), ctx->string_parser(),
                n("un_exp"),
					// n("prefixexp"), // 后面要紧跟着bin_exp的下半部分
                n("functioncall"),
                n("suffixedexp"), n("prefixexp"), n("tableconstructor"),
				// 单行lambda表达式只支持单行的直接跟表达式的语法 (args) => exp
				ctx->p_and(n("funcbody_args_def"), s("=>"), n("exp"))->set_items_in_same_line()->as("lambda_value_expr"),
				// 多行lambda表达式 (args) => do block end
				ctx->p_and(n("funcbody_args_def"), s("=>"), s("do"), n("block"), s("end"))->as("lambda_expr"),
					ctx->p_and(s("("), n("simpleexp"), s(")"))
                ));
            ctx->cache("exp",
                ctx->p_or(
                ctx->p_and(n("simpleexp"), ctx->repeatPlus(ctx->p_and(n("binop"), n("exp"))))->as("bin_exp"),
                n("simpleexp"),
					ctx->p_and(s("("), n("exp"), s(")")))); // TODO: 重构成直接定义尾递归
            /*
            auto *exp_p = ctx->cache("exp",
            ctx->or(
            s("nil"), s("false"), s("true"),
            s("..."), n("functiondef"),
            ctx->and(n("exp"), n("binop"), n("exp"))->as("bin_exp")->set_ignore_parsing_level_when_matched(true),
            ctx->number_parser(), ctx->string_parser(),
            ctx->and(n("unop"), n("exp"))->as("un_exp"),
            n("suffixedexp"), n("prefixexp"), n("tableconstructor")
            ));
            */
            /*
            std::vector<ParserFunctor*> exp_new_parsers;
            ((UnionParserFunctor*)exp_p)->remove_direct_left_recur_rule(exp_new_parsers);
            ((UnionParserFunctor*)exp_p)->set_parsers(exp_new_parsers);
            */


            ctx->cache("explist",
                ctx->p_and(n("exp"),
                ctx->optional_parser(
                ctx->p_and(s(","), n("explist")))));
            ctx->cache("args",
                ctx->p_or(ctx->p_and(s("("), ctx->optional_parser(n("explist")), s(")")),
                n("tableconstructor"), ctx->string_parser(), n("exp"))); // 这里加上n("exp")是因为如果直接用exp做参数，是为了在exp不是字符串或者tablecontructor时编译期报错，否则会被识别为2个stat
            auto *functioncall_p = ctx->cache("functioncall",
                ctx->p_or(
                ctx->p_and(n("var"), n("args"))->set_items_in_same_line()
                ->set_result_requirement([this](Input &input, MatchResult *mr)
            {
                assert(mr->is_complex());
                auto *cmr = mr->as_complex();
                auto *args_mr = cmr->get_item(1);
                if (args_mr->head_token().type == '(')
                    return true;
                if ((args_mr->is_final()
                    && args_mr->as_final()->token.type == TOKEN_RESERVED::LTK_STRING)
                    || (args_mr->node_name() == "tableconstructor" && args_mr->head_token().token=="{")) 
                    return true;
				if (args_mr->node_name() == "tableconstructor" && args_mr->head_token().token == "[")
					return false;
                // TODO: parse时stat之间间隔要有分号或者换行
				if (args_mr->node_name() == "prefixexp")
					return true;
                return false; // 这里允许不带括号调用函数的参数使用字符串字面量和table字面量外的其他exp，是为了语法parse时不把函数名和参数当成2个stat,然后提供更好的报错
            })
                , ctx->p_and(n("var"), s(":"), ctx->lua_symbol_parser(), n("args"))
				, ctx->p_and(n("simple_type"), n("args"))->set_items_in_same_line()->set_result_requirement([this](Input &input, MatchResult *mr)
            {
				auto *args_mr = mr->as_complex()->get_item(1);
				return args_mr->head_token().token == "(" || (args_mr->node_name() == "tableconstructor" && args_mr->head_token().token == "{"); // TODO: 考虑支持构造函数后直接跟着 tableconstructor
            })
                ));
			ctx->cache("type_declare_prefix",
				ctx->p_or(s(":"), s("?:")));
            ctx->cache("fieldsel",
                ctx->p_and(ctx->p_or(s("."), n("type_declare_prefix")), ctx->lua_symbol_parser()));
            ctx->cache("prefixexp",
                ctx->p_or(ctx->lua_symbol_parser(),
                ctx->p_and(s("("), n("exp"), s(")"))));
            ctx->cache("suffixedexp",
                ctx->p_or(
                ctx->p_and(n("prefixexp"),
                ctx->repeatPlus(
                ctx->p_or(ctx->p_and(s("."), ctx->lua_symbol_parser()),
                ctx->p_and(s("["), n("exp"), s("]")))
                ))->as("suffixedexp_visit_prop"),
                n("functioncall")
                // ctx->and(n("prefixexp"), ctx->and(s(":"), ctx->lua_symbol_parser(), n("args"))),
                // ctx->and(n("prefixexp"), n("args")) // FIXME: exp, suffixedexp, args，有间接循环左递归，考虑用调用深度限制来排除掉
                ));
            ctx->cache("var",
                ctx->p_or(
                ctx->p_and(n("prefixexp"), ctx->repeatPlus(ctx->p_or(
                ctx->p_and(s("."), ctx->p_or(ctx->lua_symbol_parser(), s("type"), s("default"))),
                ctx->p_and(s("["), n("exp"), s("]"))
                )))
                , n("prefixexp")
                ));
            ctx->cache("varlist",
                ctx->p_and(n("var"),
                ctx->repeatStar(ctx->p_and(s(","), n("var")))));
            ctx->cache("funcname",
                ctx->p_and(n("varname"),
                ctx->repeatStar(ctx->p_and(s("."), n("varname"))),
                ctx->optional_parser(ctx->p_and(s(":"), n("varname")))));
            ctx->cache("label",
                ctx->p_and(s("::"), ctx->lua_symbol_parser(), s("::")));
            ctx->cache("retstat",
                ctx->p_and(s("return"), ctx->optional_parser(n("explist")), ctx->optional_parser(s(";"))));
            auto *record_p = ctx->cache("record",
                ctx->p_and(s("type"), ctx->lua_symbol_parser(), s("="), s("{"), ctx->optional_parser(n("namelist_in_record")), s("}")));
            ctx->cache("generic_list",
                ctx->p_and(ctx->lua_symbol_parser(), ctx->repeatStar(ctx->p_and(s(","), ctx->lua_symbol_parser()))));
            ctx->cache("record_with_generic",
                ctx->p_and(s("type"), ctx->lua_symbol_parser(), s("<"), n("generic_list"), s(">"),
                s("="), s("{"), ctx->optional_parser(n("namelist_in_record")), s("}")));
            ctx->cache("record_with_generic_typename",
                ctx->p_and(ctx->lua_symbol_parser(), s("<"), n("generic_list"), s(">")));
            ctx->cache("typedef",
                ctx->p_and(s("type"), ctx->p_or(
                n("record_with_generic_typename"),
                ctx->lua_symbol_parser()),
                s("="), ctx->p_or(n("literal_type"), n("type"))));
            ctx->cache("generic_instance_list",
                ctx->p_and(n("type"), ctx->repeatStar(ctx->p_and(s(","), n("type")))));
			ctx->cache("literal_value_type",
				ctx-> p_or(ctx->number_parser(), ctx->string_parser(), s("true"), s("false")));
			ctx->cache("literal_type",
				ctx-> p_and (
					ctx->p_or (ctx->number_parser(), ctx->string_parser(), s("true"), s("false"), s("nil"))
					, ctx->repeatPlus(ctx-> p_and (s("|"), ctx -> p_or (ctx->number_parser(), ctx->string_parser(), s("true"), s("false"), s("nil"))))
					));
            ctx->cache("simple_type",
                ctx->p_or(
                ctx->p_and(s("Array"), s("<"), n("type"), s(">"))->as("array_type"),
				ctx->p_and(s("Map"), s("<"), n("type"), s(">"))->as("map_type"),
                ctx->p_and(ctx->lua_symbol_parser(), ctx->optional_parser(ctx->p_and(s("<"), n("generic_instance_list"), s(">")))),
				ctx->p_and(s("("), ctx->optional_parser(ctx->p_and(
                ctx->lua_symbol_parser(), // FIXME: 这里要支持复杂泛型类型
                ctx->repeatStar(ctx->p_and(s(","), ctx->lua_symbol_parser()))))->as("function_type_args"),
                s(")"), s("=>"), ctx->lua_symbol_parser())->as("function_type"),
					n("literal_value_type")
                ));
            ctx->cache("type", ctx->p_or(
                ctx->p_and(n("simple_type"), ctx->repeatPlus(ctx->p_and(s("|"), n("simple_type")))),
                n("simple_type")
                ));
			ctx->cache("emit_stat", ctx->p_and (
				s("emit"), ctx->lua_symbol_parser(), s("("), n("exp"), s(")")
				));
			ctx->cache("varname", ctx->p_or(
				ctx->lua_symbol_parser(),
				// 这里列出一些常误用成变量名的关键字，从而提供更友好的错误提示
				s("do"), 
				s("end"),
				s("repeat"),
				s("function"),
				s("emit"),
				s("type"),
				s("goto"),
				s("return"),
				s("if"),
				s("else"),
				s("until"),
				s("true"),
				s("false"),
				s("nil"),
				s("then"),
				s("and"),
				s("or"),
				s("elseif"),
				s("break"),
				s("default"),
				s("in")
			));
            ctx->cache("stat",
                ctx->p_or({
                s(";"),
                n("retstat"),
                ctx->p_and(n("var_declare_prefix"), s("function"), n("varname"), n("funcbody"))->as("local_named_function_def_stat"),
                ctx->p_and(n("var_declare_prefix"), n("namelist"), ctx->optional_parser(ctx->p_and(s("="), n("explist"))))->as("local_def_stat"),
                ctx->p_and(s("offline"), s("function"), n("funcname"), n("funcbody"))->as("offline_named_function_def_stat"),
                n("record"),
                n("record_with_generic"),
                n("typedef"),
				n("emit_stat"),
                ctx->p_and(n("varlist"), s("="), n("explist"))->as("varlist_assign_stat"),
                n("functioncall"),
                n("label"),
                s("break")->as("break_stat"), // TODO: use goto to generate break
                ctx->p_and(s("goto"), ctx->lua_symbol_parser())->as("goto_stat"),
                ctx->p_and(s("do"), n("block"), s("end"))->as("do_stat"),
                ctx->p_and(s("while"), n("exp"), s("do"), n("block"), s("end"))->as("while_stat"),
                ctx->p_and(s("repeat"), n("block"), s("until"), n("exp"))->as("repeat_stat"),
                ctx->p_and(s("if"), n("exp"), s("then"), n("block"), ctx->repeatStar(ctx->p_and(s("elseif"), n("exp"), s("then"), n("block"))),
					ctx->optional_parser(ctx->p_and(s("else"), n("block"))), s("end"))->as("if_stat")->post(
						[this](ParserFunctor *pf, MatchResult *origin) { return this->make_if_ast_node(pf, origin); }) ,
                ctx->p_and(s("for"), ctx->lua_symbol_parser(), s("="), n("explist"), s("do"), n("block"), s("end"))->as("for_step_stat"),
                ctx->p_and(s("for"), n("namelist"), s("in"), n("explist"), s("do"), n("block"), s("end"))->as("for_range_stat"),
                ctx->p_and(s("function"), n("funcname"), n("funcbody"))->as("named_function_def_stat")
                , n("exp") // FIXME: 单独变量不能作为一个stat，暂时这里是为了更好做错误提示，在typechecker部分报错而不是不parse
                ->post([](ParserFunctor *p, MatchResult *mr)
                {
                    auto items = mr->as_complex()->items;
                    auto it = items.begin();
                    std::advance(it, 2);
                    auto funcname_mr = mr->as_complex()->get_item(2);
                    std::string str1;
                    std::string str2;
                    if (funcname_mr->as_complex()->items.size() < 3)
                    {
                        str1 = funcname_mr->as_complex()->get_item(0)->as_final()->token.token;
                        str2 = "";
                    }
                    else
                    {
                        str1 = funcname_mr->as_complex()->get_item(0)->as_final()->token.token;
                        str2 = funcname_mr->as_complex()->get_item(2)->as_final()->token.token;
                    }
                    std::cout << "found offline api " << str1 + (str2.length() > 0 ? (":" + str2) : str2) << std::endl;
                    return mr;
                }),
                    n("prefixexp")
            })->lookahead());
            ctx->cache("name",
                ctx->p_or(
                ctx->p_and(n("varname"), n("type_declare_prefix"), n("type"),
						ctx->optional_parser(ctx->p_and(s("default"), n("exp"))))->as("name_with_type_declare")
                , n("varname")
                ));
			ctx->cache("name_in_record",
				ctx->p_or(
					ctx->p_and(n("varname"), n("type_declare_prefix"), n("type"),
						ctx->optional_parser(ctx->p_and(ctx->p_or(s("default"), s("=")), n("exp"))))->as("name_in_record_with_type_declare")
					, n("varname")
				));
            ctx->cache("namelist",
                ctx->p_and(n("name"), ctx->optional_parser(ctx->p_and(s(","), n("namelist"))))
                );
			ctx->cache("namelist_in_record",
				ctx->p_and(n("name_in_record"), ctx->optional_parser(ctx->p_and(s(","), n("namelist_in_record"))))
			);
            ctx->cache("block",
                ctx->p_and(ctx->repeatStar(n("stat")), ctx->optional_parser(n("retstat"))));
            auto *chunk_p = ctx->cache("chunk", n("block"));
            auto chunk_p_str = chunk_p->str();
            auto functioncall_p_str = functioncall_p->str();

            if (false)
            {
                ctx->clear_caches();
                _lua_parser = record_p;
                return _lua_parser;
            }

            ctx->clear_caches();
            _lua_parser = chunk_p;
            return _lua_parser;
        }

        ParserFunctor *ConcatParserFunctor::head()
        {
            if (_parsers.size() < 1)
                return nullptr;
            auto *p = *_parsers.begin();
            if (p->is_cached())
                return _ctx->get_cached(((CachedParserFunctor*)p)->name());
            else
                return p;
        }

        std::vector<ParserFunctor*> ConcatParserFunctor::tail()
        {
            if (_parsers.size() < 1)
                return std::vector<ParserFunctor*>();
            auto it = _parsers.begin();
            std::vector<ParserFunctor*> tail_items;
            ++it;
            while (it != _parsers.end())
            {
                auto *p = *it;
                if (p->is_cached())
                    tail_items.push_back(_ctx->get_cached(((CachedParserFunctor*)p)->name()));
                else
                    tail_items.push_back(p);
                ++it;
            }
            return tail_items;
        }

        ParseResult UnionParserFunctor::try_parse_with_lookahead_optimized(Input &input, bool optional)
        {
            if (input.can_read(1))
            {
                auto token = input.read();
                auto found1 = _parsers_started_with_final.find(token.token);
                if (found1 != _parsers_started_with_final.end())
                {
                    auto list = found1->second;
                    for (auto i = list->begin(); i != list->end(); ++i)
                    {
                        auto *p = *i;
                        auto pr = _ctx->parse_with_cache(p, input, 0, optional);
                        if (pr.state() == ParserState::SUCCESS)
                        {
                            after_match(input, pr);
                            return pr;
                        }
                    }
                }
            }
            auto _parsers = _parsers_not_started_with_final;
            assert(_parsers.size() > 0);
            if (_parsers.size() == 1)
            {
                auto pr = _ctx->parse_with_cache(*_parsers.begin(), input, 0, optional);
                after_match(input, pr);
                return pr;
            }
            auto it = _parsers.begin();
            ParseResult pr = _ctx->parse_with_cache(*it, input, 0, optional);
            if (pr.state() == SUCCESS)
            {
                after_match(input, pr);
                return pr;
            }
            if (pr.state() == ERROR)
                return pr;
            ++it;
            while (it != _parsers.end())
            {
                auto *p = *it;
                assert(nullptr != p);
                if (p->is_cached())
                {
                    auto *cached_p = (CachedParserFunctor*)p;
                    assert(cached_p->exist());
                }
                pr = _ctx->parse_with_cache(p, input, 0, optional);
                if (pr.state() != FAILURE)
                {
                    after_match(input, pr);
                    return pr;
                }
                ++it;
            }
            return pr;
        }

		static bool contains_vector_in_get_parsers_having_first_token_cache(ParsersHavingFirstTokenCacheT &items,
			std::vector<size_t> tokens_positions)
        {
	        for(const auto &item : items)
	        {
				if (item.first == tokens_positions)
					return true;
	        }
			return false;
        }

		static bool append_item_in_vector_in_get_parsers_having_first_token_cache(ParsersHavingFirstTokenCacheT &items,
			std::vector<size_t> tokens_positions, std::vector<ParserFunctor*> &result)
		{
			for (const auto &item : items)
			{
				if (item.first == tokens_positions)
					return false;
			}
			items.push_back(std::make_pair(tokens_positions, result));
			return true;
		}

		static std::vector<ParserFunctor*> get_item_in_vector_in_get_parsers_having_first_token_cache(ParsersHavingFirstTokenCacheT &items,
			std::vector<size_t> tokens_positions)
		{
			for (const auto &item : items)
			{
				if (item.first == tokens_positions)
					return item.second;
			}
			throw glua::core::GluaException("Can't find item in cache");
		}

		std::vector<ParserFunctor*> ParserFunctor::get_parsers_having_first_token(
			ParserContext *ctx, ParserFunctor *parsers_key, ParserListT &parsers, std::vector<GluaParserToken> first_tokens)
        {
			// TODO: cache
			std::vector<size_t> first_tokens_positions;
			for(const auto &token : first_tokens)
			{
				first_tokens_positions.push_back(token.position);
			}
			if (ctx->_get_parsers_having_first_token_cache.find(parsers_key) != ctx->_get_parsers_having_first_token_cache.end()
				&& contains_vector_in_get_parsers_having_first_token_cache(ctx->_get_parsers_having_first_token_cache[parsers_key], first_tokens_positions)) {
				return get_item_in_vector_in_get_parsers_having_first_token_cache(ctx->_get_parsers_having_first_token_cache[parsers_key], first_tokens_positions);
			} else
			{
				if (ctx->_get_parsers_having_first_token_cache.find(parsers_key) == ctx->_get_parsers_having_first_token_cache.end())
				{
					ctx->_get_parsers_having_first_token_cache[parsers_key] = std::vector<std::pair<std::vector<size_t>,
						std::vector<ParserFunctor*>>>();
				}

				std::vector<ParserFunctor*> result;
				for (const auto &p : parsers)
				{
					if (p->is_empty_parser())
					{
						result.push_back(p);
						continue;
					}
					auto res = p->can_take_first_token(first_tokens);
					auto lookahead_tokens_not_eos = std::count_if(first_tokens.begin(), first_tokens.end(),
						[](GluaParserToken token)
					{
						return token.type != TOKEN_RESERVED::LTK_EOS;
					});
					/*if (res>= lookahead_tokens_not_eos)
					result.push_back(p);*/
					// FIXME: 要改成排除确定失败过（有concat失败token）
					if (p->is_fixed_length() && res < lookahead_tokens_not_eos && res < p->min_length())
						continue;
					if (res >= 1)
						result.push_back(p);
				}
				append_item_in_vector_in_get_parsers_having_first_token_cache(ctx->_get_parsers_having_first_token_cache[parsers_key], first_tokens_positions, result);
				return result;
			}
        }

		ParseResult UnionParserFunctor::tryParse(Input &input, size_t skip_end_size, bool optional)
		{
			_ctx->incr_parser_functor_executed_count("union");
			// _ctx->set_last_error_token(input.read());
			_ctx->set_last_match_token(input.read());
			ParserContextParsingLevelScope level_scope(_ctx);
			if (_ctx->parsing_level_overflow(&input))
			{
				return _ctx->failure_parser()->tryParse(input, 0, optional);
			}
			assert(_parsers.size() > 0);
			if (_parsers.size() == 1)
			{
				auto pr = _ctx->parse_with_cache(*_parsers.begin(), input, skip_end_size, optional);
				after_match(input, pr);
				return pr;
			}
			if (!input.can_read(min_length()))
			{
				return _ctx->failure_parser()->tryParse(input, 0, optional);
			}


			if (node_name() == "var_declare_prefix")
				printf("");

			auto first_token = input.read(skip_end_size);
			auto lookahead_tokens = input.lookahead_tokens(4, skip_end_size);
			std::vector<GluaParserToken> first_tokens;
			first_tokens.emplace_back(first_token);
			glua::util::append_range(first_tokens, lookahead_tokens);

			if (first_token.token == "function")
				printf("");

			// FIXME: 使用更多lookahead tokens
			/*auto may_take_first_token_parsers = ParserFunctor::get_parsers_having_first_token(_ctx, this, _parsers, first_tokens);

			if (first_token.linenumber >= 6)
				printf("");
			// 根据curren和lookahead的token筛选
			if(may_take_first_token_parsers.size() == 0)
			{
				return _ctx->failure_parser()->tryParse(input, 0, optional);
			}
			if(may_take_first_token_parsers.size() == 1)
			{
				auto pr = _ctx->parse_with_cache(may_take_first_token_parsers[0], input, skip_end_size, optional);
				if (pr.state() == ParserState::SUCCESS)
				{
					after_match(input, pr);
					return pr;
				}
			}
			*/
			auto &to_iterate_parsers = _parsers; // FIXME: change to may_take_first_token_parsers
            auto it = to_iterate_parsers.begin();
			
            ParseResult pr = _ctx->parse_with_cache(*it, input, skip_end_size, true);
            if (pr.state() == ParserState::SUCCESS)
            {
                after_match(input, pr);
                return pr;
            }
            if (pr.state() == ParserState::ERROR)
                return pr;
            if (_lookahead_optimized)
                return try_parse_with_lookahead_optimized(input, true);
            ++it;
            while (it != to_iterate_parsers.end())
            {
                auto *p = *it;
                assert(nullptr != p);
                if (p->is_cached())
                {
                    auto *cached_p = (CachedParserFunctor*)p;
                    assert(cached_p->exist());
                }
				// FIXME: pprint(abc)的时候好像有问题
                pr = _ctx->parse_with_cache(p, input, skip_end_size, true);
                if (pr.state() != FAILURE)
                {
                    after_match(input, pr);
                    return pr;
                }
                ++it;
            }
            return pr;
        }

        ParserFunctor *ParserContext::concat_parsers(std::vector<ParserFunctor*> &parsers)
        {
            assert(parsers.size() > 0);
            if (parsers.size() == 1)
                return *parsers.begin();
            ConcatParserFunctor *fn = new ConcatParserFunctor(this, parsers);
            add_parser_functor(fn);
            return fn;
        }

        ParseResult ParserContext::parse_syntax(Input &input)
        {
			_get_parsers_having_first_token_cache.clear();
            auto pr = _lua_parser->tryParse(input, 0, false);
            bool success = pr.state() == ParserState::SUCCESS && pr.result()->next_input().pos() >= input.source()->size() && get_error().length() < 1;
            if (!success)
            {
                // 如果pr.state是SUCCESS，则用last_error_token，如果pr.state不是SUCCESS,则用last_match_token
                std::cout << this->simple_token_error() << std::endl;
                pr.set_state(ParserState::FAILURE);
            }
            else
            {
				pr.set_result(pr.result()->invoke_parser_post_callback());
            }
            return pr;
        }

        ParserFunctor *ParserContext::union_parsers(std::vector<ParserFunctor*> &parsers)
        {
            assert(parsers.size() > 0);
            if (parsers.size() == 1)
                return *parsers.begin();
            UnionParserFunctor *fn = new UnionParserFunctor(this, parsers);
            add_parser_functor(fn);
            return fn;
        }

        ParserFunctor *ParserContext::named_parser(std::string name)
        {
            auto found = _named_parser_functors.find(name);
            if (found != _named_parser_functors.end())
                return found->second;
            else
            {
                auto *fp = new CachedParserFunctor(this, name);
                add_parser_functor(fp);
                _named_parser_functors[name] = fp;
                return fp;
            }
        }

        ParserFunctor *ParserContext::p_and(ParserFunctor *parser1, ParserFunctor *parser2,
            ParserFunctor *parser3, ParserFunctor *parser4, ParserFunctor *parser5,
            ParserFunctor *parser6, ParserFunctor *parser7, ParserFunctor *parser8,
            ParserFunctor *parser9, ParserFunctor *parser10, ParserFunctor *parser11,
            ParserFunctor *parser12, ParserFunctor *parser13, ParserFunctor *parser14,
            ParserFunctor *parser15, ParserFunctor *parser16, ParserFunctor *parser17)
        {
            std::vector<ParserFunctor*> parsers;
            parsers.push_back(parser1);
            parsers.push_back(parser2);
            if (parser3)
                parsers.push_back(parser3);
            if (parser4)
                parsers.push_back(parser4);
            if (parser5)
                parsers.push_back(parser5);
            if (parser6)
                parsers.push_back(parser6);
            if (parser7)
                parsers.push_back(parser7);
            if (parser8)
                parsers.push_back(parser8);
            if (parser9)
                parsers.push_back(parser9);
            if (parser10)
                parsers.push_back(parser10);
            if (parser11)
                parsers.push_back(parser11);
            if (parser12)
                parsers.push_back(parser12);
            if (parser13)
                parsers.push_back(parser13);
            if (parser14)
                parsers.push_back(parser14);
            if (parser15)
                parsers.push_back(parser15);
            if (parser16)
                parsers.push_back(parser16);
            if (parser17)
                parsers.push_back(parser17);
            return concat_parsers(parsers);
        }

		ParserFunctor* ParserContext::subtract(ParserFunctor* main_parser, ParserFunctor* parser1)
		{
			auto *parser = new SubtractionParserFunctor(this, main_parser, { parser1 });
			add_parser_functor(parser);
			return parser;
		}

		ParserFunctor* ParserContext::subtract(std::vector<ParserFunctor*> const parsers)
        {
			assert(parsers.size() > 0);
			auto *main_parser = parsers.at(0);
			std::vector<ParserFunctor*> subtracted_parsers;
			for(size_t i=1;i<parsers.size();++i)
			{
				subtracted_parsers.push_back(parsers.at(i));
			}
			auto *parser = new SubtractionParserFunctor(this, main_parser, subtracted_parsers);
			add_parser_functor(parser);
			return parser;
        }


        ParserFunctor *ParserContext::p_or(ParserFunctor *parser1, ParserFunctor *parser2,
            ParserFunctor *parser3, ParserFunctor *parser4, ParserFunctor *parser5,
            ParserFunctor *parser6, ParserFunctor *parser7, ParserFunctor *parser8,
            ParserFunctor *parser9, ParserFunctor *parser10, ParserFunctor *parser11,
            ParserFunctor *parser12, ParserFunctor *parser13, ParserFunctor *parser14,
            ParserFunctor *parser15, ParserFunctor *parser16, ParserFunctor *parser17,
            ParserFunctor *parser18, ParserFunctor *parser19, ParserFunctor *parser20,
            ParserFunctor *parser21, ParserFunctor *parser22, ParserFunctor *parser23)
        {
            std::vector<ParserFunctor*> parsers;
            parsers.push_back(parser1);
            parsers.push_back(parser2);
            if (parser3)
                parsers.push_back(parser3);
            if (parser4)
                parsers.push_back(parser4);
            if (parser5)
                parsers.push_back(parser5);
            if (parser6)
                parsers.push_back(parser6);
            if (parser7)
                parsers.push_back(parser7);
            if (parser8)
                parsers.push_back(parser8);
            if (parser9)
                parsers.push_back(parser9);
            if (parser10)
                parsers.push_back(parser10);
            if (parser11)
                parsers.push_back(parser11);
            if (parser12)
                parsers.push_back(parser12);
            if (parser13)
                parsers.push_back(parser13);
            if (parser14)
                parsers.push_back(parser14);
            if (parser15)
                parsers.push_back(parser15);
            if (parser16)
                parsers.push_back(parser16);
            if (parser17)
                parsers.push_back(parser17);
            if (parser18)
                parsers.push_back(parser18);
            if (parser19)
                parsers.push_back(parser19);
            if (parser20)
                parsers.push_back(parser20);
            if (parser21)
                parsers.push_back(parser21);
            if (parser22)
                parsers.push_back(parser22);
            if (parser23)
                parsers.push_back(parser23);
            return union_parsers(parsers);
        }

        ParserFunctor *ParserContext::p_or(std::vector<ParserFunctor*> const parsers)
        {
            std::vector<ParserFunctor*> parser_list;
            for (const auto &item : parsers)
            {
                parser_list.push_back(item);
            }
            return union_parsers(parser_list);
        }

        ParserContext::ParserContext() : _parsing_level(0)
        {
            _failure_parser = new FailureParserFunctor(this);
            add_parser_functor(_failure_parser);
			_error_parser = new ErrorParserFunctor(this);
			add_parser_functor(_error_parser);
            _empty_parser = new EmptyParserFunctor(this);
            add_parser_functor(_empty_parser);
            _lua_symbol_parser = new LuaSymbolParserFunctor(this);
            add_parser_functor(_lua_symbol_parser);
            _string_parser = new StringParserFunctor(this);
            add_parser_functor(_string_parser);
            _number_parser = new NumberParserFunctor(this);
            add_parser_functor(_number_parser);
            _last_error_token.token = "";
            _last_error_token.source_token = "";
            _last_error_token.linenumber = 0;
            _last_error_token.position = 0;
            _last_error_token.type = (TOKEN_RESERVED)0;
            _last_second_error_token.token = "";
            _last_second_error_token.source_token = "";
            _last_second_error_token.linenumber = 0;
            _last_second_error_token.position = 0;
            _last_second_error_token.type = (TOKEN_RESERVED)0;
            _last_match_token.token = "";
            _last_match_token.source_token = "";
            _last_match_token.linenumber = 0;
            _last_match_token.position = 0;
            _last_match_token.type = (TOKEN_RESERVED)0;
        }

        ParserFunctor *ParserContext::failure_parser() const
        {
            return _failure_parser;
        }

		ParserFunctor *ParserContext::error_parser() const
		{
			return _error_parser;
		}

        ParserFunctor *ParserContext::empty_parser() const
        {
            return _empty_parser;
        }

        ParserFunctor *ParserContext::lua_symbol_parser() const
        {
            return _lua_symbol_parser;
        }

        ParserFunctor *ParserContext::string_parser() const
        {
            return _string_parser;
        }

        ParserFunctor *ParserContext::number_parser() const
        {
            return _number_parser;
        }

        ParserFunctor *ParserContext::optional_parser(ParserFunctor *parser)
        {
            return p_or(parser, empty_parser());
        }

        ParserFunctor *ParserContext::repeatStar(ParserFunctor *parser)
        {
            RepeatStarParserFunctor *fn = new RepeatStarParserFunctor(this, parser);
            add_parser_functor(fn);
            return fn;
        }

        ParserFunctor *ParserContext::repeatPlus(ParserFunctor *parser)
        {
            return p_and(parser, repeatStar(parser));
        }

        ParserFunctor *ParserContext::repeatCount(ParserFunctor *parser, size_t n)
        {
            if (n < 1)
                return empty_parser();
            if (n == 1)
                return parser;
            auto *pf = new RepeatCountParserFunctor(this, parser, n);
            add_parser_functor(pf);
            return pf;
        }

        ParserFunctor *ParserContext::literal(std::string &token_str)
        {
            if (_literal_parser_functors.find(token_str) != _literal_parser_functors.end())
            {
                return _literal_parser_functors[token_str];
            }
            ParserFunctor *pf = new LiteralParserFunctor(this, token_str);
            add_parser_functor(pf);
            _literal_parser_functors[token_str] = pf;
            return pf;
        }

        ParserFunctor *ParserContext::token_type_parser(int type)
        {
            if (_token_type_parser_functors.find(type) != _token_type_parser_functors.end())
            {
                return _token_type_parser_functors[type];
            }
            ParserFunctor *pf = new LuaTokenTypeParserFunctor(this, type);
            add_parser_functor(pf);
            _token_type_parser_functors[type] = pf;
            return pf;
        }

        Input FinalMatchResult::next_input() const
        {
            return _next_input;
        }

        Input ComplexMatchResult::next_input() const
        {
            return _next_input;
        }

    } // end glua::parser
}