#include "magma.h"

static
bool_t
is_locked(meta_user_t *user) {
	return user->lock_status < 5;
}

static
chr_t *
lock_error_message(meta_user_t *user)
{
	chr_t *result;

	switch (user->lock_status) {
		case 0:
			result = "This account has been locked.";
			break;
		case 1:
			result = "This account has been administratively locked.";
			break;
		case 2:
			result = "This account has been locked for inactivity.";
			break;
		case 3:
			result = "This account has been locked on suspicion of abuse.";
			break;
		case 4:
			result = "This account has been locked at the request of the user.";
			break;
		default:
			break;
	}
}

void
api_endpoint_auth(connection_t *con) {
	size_t count;
	json_error_t jansson_err;
	chr_t *username;
	chr_t *password;
	meta_user_t *user;

	if (
		json_unpack_ex_d(
			con->http.portal.params,
			&jansson_err,
			JSON_STRICT,
			"{s:s, s:s}",
			"username", &username,
			"password", &password)
		!= 0)
	{
		log_pedantic(
			"Received invalid portal auth request parameters "
			"{ user = %s, errmsg = %s }",
			st_char_get(con->http.session->user->username),
			jansson_err.text);

		api_error(
			con,
			HTTP_ERROR_400,
			JSON_RPC_2_ERROR_SERVER_METHOD_PARAMS,
			"Invalid method parameters.");

		goto out;
	}

	// TODO! - wire this up here!
	//if (
	//	!authenticate_stub_REPLACE_ME(
	//		user,
	//		username,
	//		password,
	//		META_PROT_JSON))
	//{
	//	api_error(
	//		con,
	//		HTTP_ERROR_400,
	//		PORTAL_ENDPOINT_ERROR_AUTH,
	//		"Unable to authenticate with given username and password.");
	//	goto cleanup_username_password;
	//}

	if (is_locked(user)) {
		api_error(
			con,
			HTTP_ERROR_400,
			PORTAL_ENDPOINT_ERROR_AUTH,
			lock_error_message(user));
		goto cleanup_user;
	}

	con->http.session->state = SESSION_STATE_AUTHENTICATED;
	con->http.response.cookie = HTTP_COOKIE_SET;
	portal_endpoint_response(
		con,
		"{s:s, s:{s:s, s:s}, s:I}",
		"jsonrpc", "2.0",
		"result",
			"auth", "success",
			"session", st_char_get(con->http.session->warden.token),
		"id", con->http.portal.id);

	con->http.session->user = user;

	api_response(
		con,
		HTTP_OK,
		"{s:s, s:I}",
		"jsonrpc", "2.0",
		"id", con->http.portal.id);

cleanup_user:
	meta_remove(user->username, META_PROT_JSON);
cleanup_username_password:
	ns_free(username);
	ns_free(password);
out:
	return;
}

void
api_endpoint_register(connection_t *con) {
	json_error_t jansson_err;
	chr_t *username;
	chr_t *password;
	chr_t *password_verification;

	int64_t transaction;
	uint64_t usernum = 0;

	if (
		json_unpack_ex_d(
			con->http.portal.params,
			&jansson_err,
			JSON_STRICT,
			"{s:s, s:s, s:s}",
			"username", &username,
			"password", &password,
			"password_verification", &password_verification)
		!= 0)
	{
		log_pedantic(
			"Received invalid portal auth request parameters "
			"{ user = %s, errmsg = %s }",
			st_char_get(con->http.session->user->username),
			jansson_err.text);

		api_error(
			con,
			HTTP_ERROR_400,
			JSON_RPC_2_ERROR_SERVER_METHOD_PARAMS,
			"Invalid method parameters.");

		goto out;
	}

	// Start the transaction.
	transaction = tran_start();
	if (transaction == -1) {
		api_error(
			con,
			HTTP_ERROR_500,
			JSON_RPC_2_ERROR_SERVER_INTERNAL,
			"Internal server error.");
		goto cleanup;
	}

	// Database insert.
	if (
		!register_data_insert_user(
			con,
			1,
			NULLER(username),
			NULLER(password),
			transaction,
			&usernum))
	{
		tran_rollback(transaction);
		api_error(
			con,
			HTTP_ERROR_500,
			JSON_RPC_2_ERROR_SERVER_INTERNAL,
			"Internal server error.");
		goto cleanup;
	}

	// Were finally done.
	tran_commit(transaction);

	// And finally, increment the abuse counter.
	register_abuse_increment_history(con);

	api_response(
		con,
		HTTP_OK,
		"{s:s, s:I}",
		"jsonrpc", "2.0",
		"id", con->http.portal.id);

cleanup:
	ns_free(username);
	ns_free(password);
	ns_free(password_verification);
out:
	return;
}

void
api_endpoint_change_password(connection_t *con) {
	json_error_t jansson_err;
	chr_t *password;
	chr_t *new_password;
	chr_t *new_password_verification;

	if (
		json_unpack_ex_d(
			con->http.portal.params,
			&jansson_err,
			JSON_STRICT,
			"{s:s, s:s, s:s}",
			"password", &password,
			"new_password", &new_password,
			"new_password_verification", &new_password_verification)
		!= 0)
	{
		log_pedantic(
			"Received invalid portal auth request parameters "
			"{ user = %s, errmsg = %s }",
			st_char_get(con->http.session->user->username),
			jansson_err.text);

		api_error(
			con,
			HTTP_ERROR_400,
			JSON_RPC_2_ERROR_SERVER_METHOD_PARAMS,
			"Invalid method parameters.");

		goto out;
	}

	// TODO - wire up here

cleanup:
	ns_free(password);
	ns_free(new_password);
	ns_free(new_password_verification);
out:
	return;
}
